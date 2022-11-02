/*
 This source file is part of the Swift.org open source project

 Copyright (c) 2014 - 2022 Apple Inc. and the Swift project authors
 Licensed under Apache License v2.0 with Runtime Library Exception

 See http://swift.org/LICENSE.txt for license information
 See http://swift.org/CONTRIBUTORS.txt for Swift project authors
*/

import Foundation
import TSCLibc

@available(*, deprecated, message: "Use NSLock directly instead. SPM has a withLock extension function" )
/// A simple lock wrapper.
public struct Lock {
    private let _lock = NSLock()

    /// Create a new lock.
    public init() {
    }

    func lock() {
        _lock.lock()
    }

    func unlock() {
        _lock.unlock()
    }

    /// Execute the given block while holding the lock.
    public func withLock<T> (_ body: () throws -> T) rethrows -> T {
        lock()
        defer { unlock() }
        return try body()
    }
}

// for internal usage
extension NSLock {
    internal func withLock<T> (_ body: () throws -> T) rethrows -> T {
        self.lock()
        defer { self.unlock() }
        return try body()
    }
}

enum ProcessLockError: Error {
    case unableToAquireLock(errno: Int32)
}

extension ProcessLockError: CustomNSError {
    public var errorUserInfo: [String : Any] {
        return [NSLocalizedDescriptionKey: "\(self)"]
    }
}
/// Provides functionality to acquire a lock on a file via POSIX's flock() method.
/// It can be used for things like serializing concurrent mutations on a shared resource
/// by multiple instances of a process. The `FileLock` is not thread-safe.
public final class FileLock {

    public enum LockType {
        case exclusive
        case shared
    }

    /// File descriptor to the lock file.
  #if os(Windows)
    private var handle: HANDLE?
  #else
    private var fileDescriptor: CInt?
  #endif

    /// Path to the lock file.
    private let lockFile: AbsolutePath

    /// Create an instance of FileLock at the path specified
    ///
    /// Note: The parent directory path should be a valid directory.
    public init(at lockFile: AbsolutePath) {
        self.lockFile = lockFile
    }

    @available(*, deprecated, message: "use init(at:) instead")
    public convenience init(name: String, cachePath: AbsolutePath) {
        self.init(at: cachePath.appending(component: name + ".lock"))
    }

    /// Try to acquire a lock. This method will block until lock the already aquired by other process.
    ///
    /// Note: This method can throw if underlying POSIX methods fail.
    public func lock(type: LockType = .exclusive) throws {
      #if os(Windows)
        if handle == nil {
            let h: HANDLE = lockFile.pathString.withCString(encodedAs: UTF16.self, {
                CreateFileW(
                    $0,
                    UInt32(GENERIC_READ) | UInt32(GENERIC_WRITE),
                    UInt32(FILE_SHARE_READ) | UInt32(FILE_SHARE_WRITE),
                    nil,
                    DWORD(OPEN_ALWAYS),
                    DWORD(FILE_ATTRIBUTE_NORMAL),
                    nil
                )
            })
            if h == INVALID_HANDLE_VALUE {
                throw FileSystemError(errno: Int32(GetLastError()), lockFile)
            }
            self.handle = h
        }
        var overlapped = OVERLAPPED()
        overlapped.Offset = 0
        overlapped.OffsetHigh = 0
        overlapped.hEvent = nil
        switch type {
        case .exclusive:
            if !LockFileEx(handle, DWORD(LOCKFILE_EXCLUSIVE_LOCK), 0,
                           UInt32.max, UInt32.max, &overlapped) {
                throw ProcessLockError.unableToAquireLock(errno: Int32(GetLastError()))
            }
        case .shared:
            if !LockFileEx(handle, 0, 0,
                           UInt32.max, UInt32.max, &overlapped) {
                throw ProcessLockError.unableToAquireLock(errno: Int32(GetLastError()))
            }
        }
      #else
        // Open the lock file.
        if fileDescriptor == nil {
            let fd = TSCLibc.open(lockFile.pathString, O_WRONLY | O_CREAT | O_CLOEXEC, 0o666)
            if fd == -1 {
                throw FileSystemError(errno: errno, lockFile)
            }
            self.fileDescriptor = fd
        }
        // Aquire lock on the file.
        while true {
            if type == .exclusive && flock(fileDescriptor!, LOCK_EX) == 0 {
                break
            } else if type == .shared && flock(fileDescriptor!, LOCK_SH) == 0 {
                break
            }
            // Retry if interrupted.
            if errno == EINTR { continue }
            throw ProcessLockError.unableToAquireLock(errno: errno)
        }
      #endif
    }

    /// Unlock the held lock.
    public func unlock() {
      #if os(Windows)
        var overlapped = OVERLAPPED()
        overlapped.Offset = 0
        overlapped.OffsetHigh = 0
        overlapped.hEvent = nil
        UnlockFileEx(handle, 0, UInt32.max, UInt32.max, &overlapped)
      #else
        guard let fd = fileDescriptor else { return }
        flock(fd, LOCK_UN)
      #endif
    }

    deinit {
      #if os(Windows)
        guard let handle = handle else { return }
        CloseHandle(handle)
      #else
        guard let fd = fileDescriptor else { return }
        close(fd)
      #endif
    }

    /// Execute the given block while holding the lock.
    public func withLock<T>(type: LockType = .exclusive, _ body: () throws -> T) throws -> T {
        try lock(type: type)
        defer { unlock() }
        return try body()
    }
    
    public static func withLock<T>(fileToLock: AbsolutePath, lockFilesDirectory: AbsolutePath? = nil, type: LockType = .exclusive, body: () throws -> T) throws -> T {
        // unless specified, we use the tempDirectory to store lock files
        let lockFilesDirectory = lockFilesDirectory ?? localFileSystem.tempDirectory
        if !localFileSystem.exists(lockFilesDirectory) {
            throw FileSystemError(.noEntry, lockFilesDirectory)
        }
        if !localFileSystem.isDirectory(lockFilesDirectory) {
            throw FileSystemError(.notDirectory, lockFilesDirectory)
        }
        // use the parent path to generate unique filename in temp
        var lockFileName = (resolveSymlinks(fileToLock.parentDirectory)
                                .appending(component: fileToLock.basename))
                                .components.joined(separator: "_")
                                .replacingOccurrences(of: ":", with: "_") + ".lock"
#if os(Windows)
        // NTFS has an ARC limit of 255 codepoints
        var lockFileUTF16 = lockFileName.utf16.suffix(255)
        while String(lockFileUTF16) == nil {
            lockFileUTF16 = lockFileUTF16.dropFirst()
        }
        lockFileName = String(lockFileUTF16) ?? lockFileName
#else
        if lockFileName.hasPrefix(AbsolutePath.root.pathString) {
            lockFileName = String(lockFileName.dropFirst(AbsolutePath.root.pathString.count))
        }
        // back off until it occupies at most `NAME_MAX` UTF-8 bytes but without splitting scalars
        // (we might split clusters but it's not worth the effort to keep them together as long as we get a valid file name)
        var lockFileUTF8 = lockFileName.utf8.suffix(Int(NAME_MAX))
        while String(lockFileUTF8) == nil {
            // in practice this will only be a few iterations
            lockFileUTF8 = lockFileUTF8.dropFirst()
        }
        // we will never end up with nil since we have ASCII characters at the end
        lockFileName = String(lockFileUTF8) ?? lockFileName
#endif
        let lockFilePath = lockFilesDirectory.appending(component: lockFileName)

        let lock = FileLock(at: lockFilePath)
        return try lock.withLock(type: type, body)
    }
}
