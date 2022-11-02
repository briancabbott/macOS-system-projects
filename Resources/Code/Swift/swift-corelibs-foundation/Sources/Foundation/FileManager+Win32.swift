// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2019 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//

@_implementationOnly import CoreFoundation

#if os(Windows)
internal func joinPath(prefix: String, suffix: String) -> String {
    var pszPath: PWSTR?

    guard !prefix.isEmpty else { return suffix }
    guard !suffix.isEmpty else { return prefix }

    _ = try! FileManager.default._fileSystemRepresentation(withPath: prefix, andPath: suffix) {
      PathAllocCombine($0, $1, ULONG(PATHCCH_ALLOW_LONG_PATHS.rawValue), &pszPath)
    }

    let path: String = String(decodingCString: pszPath!, as: UTF16.self)
    LocalFree(pszPath)
    return path
}

extension FileManager {
    internal func _mountedVolumeURLs(includingResourceValuesForKeys propertyKeys: [URLResourceKey]?, options: VolumeEnumerationOptions = []) -> [URL]? {
        var urls: [URL] = []

        var wszVolumeName: [WCHAR] = Array<WCHAR>(repeating: 0, count: Int(MAX_PATH))

        var hVolumes: HANDLE = FindFirstVolumeW(&wszVolumeName, DWORD(wszVolumeName.count))
        guard hVolumes != INVALID_HANDLE_VALUE else { return nil }
        defer { FindVolumeClose(hVolumes) }

        repeat {
            var dwCChReturnLength: DWORD = 0
            GetVolumePathNamesForVolumeNameW(&wszVolumeName, nil, 0, &dwCChReturnLength)

            var wszPathNames: [WCHAR] = Array<WCHAR>(repeating: 0, count: Int(dwCChReturnLength + 1))
            if !GetVolumePathNamesForVolumeNameW(&wszVolumeName, &wszPathNames, DWORD(wszPathNames.count), &dwCChReturnLength) {
                // TODO(compnerd) handle error
                continue
            }

            // GetVolumePathNamesForVolumeNameW writes an array of
            // null terminated wchar strings followed by an additional
            // null terminator.
            // e.g. [ "C", ":", "\\", "\0", "D", ":", "\\", "\0", "\0"]
            var remaining = wszPathNames[...]
            while !remaining.isEmpty {
                let path = remaining.withUnsafeBufferPointer {
                    String(decodingCString: $0.baseAddress!, as: UTF16.self)
                }

                if !path.isEmpty {
                    urls.append(URL(fileURLWithPath: path, isDirectory: true))
                }
                remaining = remaining.dropFirst(path.count + 1)
            }
        } while FindNextVolumeW(hVolumes, &wszVolumeName, DWORD(wszVolumeName.count))

        return urls
    }
    internal func _urls(for directory: SearchPathDirectory, in domainMask: SearchPathDomainMask) -> [URL] {
        let domains = _SearchPathDomain.allInSearchOrder(from: domainMask)

        var urls: [URL] = []

        for domain in domains {
            urls.append(contentsOf: windowsURLs(for: directory, in: domain))
        }

        return urls
    }

    private class func url(for id: KNOWNFOLDERID) -> URL {
        var pszPath: PWSTR?
        let hResult: HRESULT = withUnsafePointer(to: id) { id in
            SHGetKnownFolderPath(id, DWORD(KF_FLAG_DEFAULT.rawValue), nil, &pszPath)
        }
        precondition(hResult >= 0, "SHGetKnownFolderpath failed \(GetLastError())")
        let url: URL = URL(fileURLWithPath: String(decodingCString: pszPath!, as: UTF16.self), isDirectory: true)
        CoTaskMemFree(pszPath)
        return url
    }

    private func windowsURLs(for directory: SearchPathDirectory, in domain: _SearchPathDomain) -> [URL] {
        switch directory {
        case .autosavedInformationDirectory:
            // FIXME(compnerd) where should this go?
            return []

        case .desktopDirectory:
            guard domain == .user else { return [] }
            return [FileManager.url(for: FOLDERID_Desktop)]

        case .documentDirectory:
            guard domain == .user else { return [] }
            return [FileManager.url(for: FOLDERID_Documents)]

        case .cachesDirectory:
            guard domain == .user else { return [] }
            return [URL(fileURLWithPath: NSTemporaryDirectory())]

        case .applicationSupportDirectory:
            switch domain {
            case .local:
                return [FileManager.url(for: FOLDERID_ProgramData)]
            case .user:
                return [FileManager.url(for: FOLDERID_LocalAppData)]
            default:
                return []
            }

        case .downloadsDirectory:
            guard domain == .user else { return [] }
            return [FileManager.url(for: FOLDERID_Downloads)]

        case .userDirectory:
            guard domain == .user else { return [] }
            return [FileManager.url(for: FOLDERID_UserProfiles)]

        case .moviesDirectory:
            guard domain == .user else { return [] }
            return [FileManager.url(for: FOLDERID_Videos)]

        case .musicDirectory:
            guard domain == .user else { return [] }
            return [FileManager.url(for: FOLDERID_Music)]

        case .picturesDirectory:
            guard domain == .user else { return [] }
            return [FileManager.url(for: FOLDERID_PicturesLibrary)]

        case .sharedPublicDirectory:
            guard domain == .user else { return [] }
            return [FileManager.url(for: FOLDERID_Public)]

        case .trashDirectory:
            guard domain == .user else { return [] }
            return [FileManager.url(for: FOLDERID_RecycleBinFolder)]

            // None of these are supported outside of Darwin:
        case .applicationDirectory,
                .demoApplicationDirectory,
                .developerApplicationDirectory,
                .adminApplicationDirectory,
                .libraryDirectory,
                .developerDirectory,
                .documentationDirectory,
                .coreServiceDirectory,
                .inputMethodsDirectory,
                .preferencePanesDirectory,
                .applicationScriptsDirectory,
                .allApplicationsDirectory,
                .allLibrariesDirectory,
                .printerDescriptionDirectory,
                .itemReplacementDirectory:
            return []
        }
    }

    internal func _createDirectory(atPath path: String, withIntermediateDirectories createIntermediates: Bool, attributes: [FileAttributeKey : Any]? = [:]) throws {
        if createIntermediates {
            var isDir: ObjCBool = false
            if fileExists(atPath: path, isDirectory: &isDir) {
                guard isDir.boolValue else { throw _NSErrorWithErrno(EEXIST, reading: false, path: path) }
                return
            }

            let parent = path._nsObject.deletingLastPathComponent
            if !parent.isEmpty && !fileExists(atPath: parent, isDirectory: &isDir) {
                try createDirectory(atPath: parent, withIntermediateDirectories: true, attributes: attributes)
            }
        }

        try FileManager.default._fileSystemRepresentation(withPath: path) { fsr in
          var saAttributes: SECURITY_ATTRIBUTES =
            SECURITY_ATTRIBUTES(nLength: DWORD(MemoryLayout<SECURITY_ATTRIBUTES>.size),
                                lpSecurityDescriptor: nil,
                                bInheritHandle: false)
          try withUnsafeMutablePointer(to: &saAttributes) {
            if !CreateDirectoryW(fsr, $0) {
              throw _NSErrorWithWindowsError(GetLastError(), reading: false, paths: [path])
            }
          }

          if let attr = attributes {
            try self.setAttributes(attr, ofItemAtPath: path)
          }
        }
    }

    internal func _contentsOfDir(atPath path: String, _ closure: (String, Int32) throws -> () ) throws {
        guard path != "" else {
            throw NSError(domain: NSCocoaErrorDomain, code: CocoaError.fileReadInvalidFileName.rawValue, userInfo: [NSFilePathErrorKey : NSString(path)])
        }
        try FileManager.default._fileSystemRepresentation(withPath: path + "\\*") {
            var ffd: WIN32_FIND_DATAW = WIN32_FIND_DATAW()

            let hDirectory: HANDLE = FindFirstFileW($0, &ffd)
            if hDirectory == INVALID_HANDLE_VALUE {
                throw _NSErrorWithWindowsError(GetLastError(), reading: true, paths: [path])
            }
            defer { FindClose(hDirectory) }

            repeat {
                let path: String = withUnsafePointer(to: &ffd.cFileName) {
                    $0.withMemoryRebound(to: UInt16.self, capacity: MemoryLayout.size(ofValue: $0) / MemoryLayout<WCHAR>.size) {
                        String(decodingCString: $0, as: UTF16.self)
                    }
                }
                if path != "." && path != ".." {
                    try closure(path.standardizingPath, Int32(ffd.dwFileAttributes))
                }
            } while FindNextFileW(hDirectory, &ffd)
        }
    }

    internal func _subpathsOfDirectory(atPath path: String) throws -> [String] {
        var contents: [String] = []

        try _contentsOfDir(atPath: path, { (entryName, entryType) throws in
            contents.append(entryName)
            if entryType & FILE_ATTRIBUTE_DIRECTORY == FILE_ATTRIBUTE_DIRECTORY
                 && entryType & FILE_ATTRIBUTE_REPARSE_POINT != FILE_ATTRIBUTE_REPARSE_POINT {
                let subPath: String = joinPath(prefix: path, suffix: entryName)
                let entries = try subpathsOfDirectory(atPath: subPath)
                contents.append(contentsOf: entries.map { joinPath(prefix: entryName, suffix: $0).standardizingPath })
            }
        })
        return contents
    }

    internal func windowsFileAttributes(atPath path: String) throws -> WIN32_FILE_ATTRIBUTE_DATA {
      return try FileManager.default._fileSystemRepresentation(withPath: path) {
        var faAttributes: WIN32_FILE_ATTRIBUTE_DATA = WIN32_FILE_ATTRIBUTE_DATA()
        if !GetFileAttributesExW($0, GetFileExInfoStandard, &faAttributes) {
          throw _NSErrorWithWindowsError(GetLastError(), reading: true, paths: [path])
        }
        return faAttributes
      }
    }

    internal func _attributesOfFileSystemIncludingBlockSize(forPath path: String) throws -> (attributes: [FileAttributeKey : Any], blockSize: UInt64?) {
        return (attributes: try _attributesOfFileSystem(forPath: path), blockSize: nil)
    }

    internal func _attributesOfFileSystem(forPath path: String) throws -> [FileAttributeKey : Any] {
        var result: [FileAttributeKey:Any] = [:]

        try FileManager.default._fileSystemRepresentation(withPath: path) {
            let dwLength: DWORD = GetFullPathNameW($0, 0, nil, nil)
            guard dwLength > 0 else {
                throw _NSErrorWithWindowsError(GetLastError(), reading: true, paths: [path])
            }

            var szVolumePath: [WCHAR] = Array<WCHAR>(repeating: 0, count: Int(dwLength + 1))
            guard GetVolumePathNameW($0, &szVolumePath, dwLength) else {
                throw _NSErrorWithWindowsError(GetLastError(), reading: true, paths: [path])
            }

            var liTotal: ULARGE_INTEGER = ULARGE_INTEGER()
            var liFree: ULARGE_INTEGER = ULARGE_INTEGER()
            guard GetDiskFreeSpaceExW(&szVolumePath, nil, &liTotal, &liFree) else {
                throw _NSErrorWithWindowsError(GetLastError(), reading: true, paths: [path])
            }

            let hr: HRESULT = PathCchStripToRoot(&szVolumePath, szVolumePath.count)
            guard hr == S_OK || hr == S_FALSE else {
                throw _NSErrorWithWindowsError(DWORD(hr & 0xffff), reading: true, paths: [path])
            }

            var volumeSerialNumber: DWORD = 0
            guard GetVolumeInformationW(&szVolumePath, nil, 0, &volumeSerialNumber, nil, nil, nil, 0) else {
                throw _NSErrorWithWindowsError(GetLastError(), reading: true, paths: [path])
            }

            result[.systemSize] = NSNumber(value: liTotal.QuadPart)
            result[.systemFreeSize] = NSNumber(value: liFree.QuadPart)
            result[.systemNumber] = NSNumber(value: volumeSerialNumber)
            // FIXME(compnerd): what about .systemNodes, .systemFreeNodes?
        }
        return result
    }

    internal func _createSymbolicLink(atPath path: String, withDestinationPath destPath: String, isDirectory: Bool? = nil) throws {
        var dwFlags = DWORD(SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)
        // If destPath is relative, we should look for it relative to `path`, not our current working directory
        switch isDirectory {
            case .some(true):
                dwFlags |= DWORD(SYMBOLIC_LINK_FLAG_DIRECTORY)
            case .some(false):
                break;
            case .none:
                let resolvedDest =
                  destPath.isAbsolutePath ? destPath
                                          : joinPath(prefix: path.deletingLastPathComponent,
                                                     suffix: destPath)

                // NOTE: windowsfileAttributes will throw if the destPath is not
                // found.  Since on Windows, you are required to know the type
                // of the symlink target (file or directory) during creation,
                // and assuming one or the other doesn't make a lot of sense, we
                // allow it to throw, thus disallowing the creation of broken
                // symlinks on Windows is the target is of unknown type.
                guard let faAttributes = try? windowsFileAttributes(atPath: resolvedDest) else {
                    throw _NSErrorWithWindowsError(GetLastError(), reading: true, paths: [path, destPath])
                }
                if faAttributes.dwFileAttributes & DWORD(FILE_ATTRIBUTE_DIRECTORY) == DWORD(FILE_ATTRIBUTE_DIRECTORY) {
                    dwFlags |= DWORD(SYMBOLIC_LINK_FLAG_DIRECTORY)
                }
        }

        try FileManager.default._fileSystemRepresentation(withPath: path, andPath: destPath) {
          guard CreateSymbolicLinkW($0, $1, dwFlags) != 0 else {
            throw _NSErrorWithWindowsError(GetLastError(), reading: true, paths: [path, destPath])
          }
        }
    }

    internal func _destinationOfSymbolicLink(atPath path: String) throws -> String {
        let faAttributes = try windowsFileAttributes(atPath: path)
        guard faAttributes.dwFileAttributes & DWORD(FILE_ATTRIBUTE_REPARSE_POINT) == DWORD(FILE_ATTRIBUTE_REPARSE_POINT) else {
            throw _NSErrorWithWindowsError(DWORD(ERROR_BAD_ARGUMENTS), reading: false)
        }

        let handle: HANDLE = try FileManager.default._fileSystemRepresentation(withPath: path) {
          CreateFileW($0, GENERIC_READ,
                      DWORD(FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE),
                      nil, DWORD(OPEN_EXISTING),
                      DWORD(FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS),
                      nil)
        }
        if handle == INVALID_HANDLE_VALUE {
            throw _NSErrorWithWindowsError(GetLastError(), reading: true)
        }
        defer { CloseHandle(handle) }

        // Since REPARSE_DATA_BUFFER ends with an arbitrarily long buffer, we
        // have to manually get the path buffer out of it since binding it to a
        // type will truncate the path buffer.
        //
        // 20 is the sum of the offsets of:
        // ULONG ReparseTag
        // USHORT ReparseDataLength
        // USHORT Reserved
        // USHORT SubstituteNameOffset
        // USHORT SubstituteNameLength
        // USHORT PrintNameOffset
        // USHORT PrintNameLength
        // ULONG Flags (Symlink only)
        let symLinkPathBufferOffset = 20 // 4 + 2 + 2 + 2 + 2 + 2 + 2 + 4
        let mountPointPathBufferOffset = 16 // 4 + 2 + 2 + 2 + 2 + 2 + 2
        let buff = UnsafeMutableRawBufferPointer.allocate(byteCount: Int(MAXIMUM_REPARSE_DATA_BUFFER_SIZE),
                                                          alignment: 8)

        guard let buffBase = buff.baseAddress else {
            throw _NSErrorWithWindowsError(DWORD(ERROR_INVALID_DATA), reading: false)
        }

        var bytesWritten: DWORD = 0
        guard DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, nil, 0,
                              buffBase, DWORD(MAXIMUM_REPARSE_DATA_BUFFER_SIZE),
                              &bytesWritten, nil) else {
            throw _NSErrorWithWindowsError(GetLastError(), reading: true)
        }

        guard bytesWritten >= MemoryLayout<REPARSE_DATA_BUFFER>.size else {
            throw _NSErrorWithWindowsError(DWORD(ERROR_INVALID_DATA), reading: false)
        }

        let bound = buff.bindMemory(to: REPARSE_DATA_BUFFER.self)
        guard let reparseDataBuffer = bound.first else {
            throw _NSErrorWithWindowsError(DWORD(ERROR_INVALID_DATA), reading: false)
        }

        guard reparseDataBuffer.ReparseTag == IO_REPARSE_TAG_SYMLINK
                || reparseDataBuffer.ReparseTag == IO_REPARSE_TAG_MOUNT_POINT else {
            throw _NSErrorWithWindowsError(DWORD(ERROR_BAD_ARGUMENTS), reading: false)
        }

        let pathBufferPtr: UnsafeMutableRawPointer
        let substituteNameBytes: Int
        let substituteNameOffset: Int
        switch reparseDataBuffer.ReparseTag {
            case IO_REPARSE_TAG_SYMLINK:
                pathBufferPtr = buffBase + symLinkPathBufferOffset
                substituteNameBytes = Int(reparseDataBuffer.SymbolicLinkReparseBuffer.SubstituteNameLength)
                substituteNameOffset = Int(reparseDataBuffer.SymbolicLinkReparseBuffer.SubstituteNameOffset)
            case IO_REPARSE_TAG_MOUNT_POINT:
                pathBufferPtr = buffBase + mountPointPathBufferOffset
                substituteNameBytes = Int(reparseDataBuffer.MountPointReparseBuffer.SubstituteNameLength)
                substituteNameOffset = Int(reparseDataBuffer.MountPointReparseBuffer.SubstituteNameOffset)
            default:
                throw _NSErrorWithWindowsError(DWORD(ERROR_BAD_ARGUMENTS), reading: false)
        }

        guard substituteNameBytes + substituteNameOffset <= bytesWritten else {
            throw _NSErrorWithWindowsError(DWORD(ERROR_INVALID_DATA), reading: false)
        }

        let substituteNameBuff = Data(bytes: pathBufferPtr + substituteNameOffset, count: substituteNameBytes)
        guard var substitutePath = String(data: substituteNameBuff, encoding: .utf16LittleEndian) else {
            throw _NSErrorWithWindowsError(DWORD(ERROR_INVALID_DATA), reading: false)
        }

        // Canonicalize the NT Object Manager Path to the DOS style path
        // instead.  Unfortunately, there is no nice API which can allow us to
        // do this in a guranteed way.
        let kObjectManagerPrefix = "\\??\\"
        if substitutePath.hasPrefix(kObjectManagerPrefix) {
          substitutePath = String(substitutePath.dropFirst(kObjectManagerPrefix.count))
        }
        return substitutePath
    }
    
    private func _realpath(_ path: String) -> String {
        return (try? _destinationOfSymbolicLink(atPath: path)) ?? path
    }
    
    internal func _recursiveDestinationOfSymbolicLink(atPath path: String) throws -> String {
        // Throw error if path is not a symbolic link:
        var previousIterationDestination = try _destinationOfSymbolicLink(atPath: path)
        
        // Same recursion limit as in Darwin:
        let symbolicLinkRecursionLimit = 32
        for _ in 0..<symbolicLinkRecursionLimit {
            let iterationDestination = _realpath(previousIterationDestination)
            if previousIterationDestination == iterationDestination {
                return iterationDestination
            }
            previousIterationDestination = iterationDestination
        }
        
        // As in Darwin Foundation, after the recursion limit we return the initial path without resolution.
        return path
    }

    internal func _canonicalizedPath(toFileAtPath path: String) throws -> String {
        let hFile: HANDLE = try FileManager.default._fileSystemRepresentation(withPath: path) {
          // BACKUP_SEMANTICS are (confusingly) required in order to receive a
          // handle to a directory
          CreateFileW($0, /*dwDesiredAccess=*/DWORD(0),
                      DWORD(FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE),
                      /*lpSecurityAttributes=*/nil, DWORD(OPEN_EXISTING),
                      DWORD(FILE_FLAG_BACKUP_SEMANTICS), /*hTemplateFile=*/nil)
        }
        if hFile == INVALID_HANDLE_VALUE {
          return try FileManager.default._fileSystemRepresentation(withPath: path) {
            var dwLength = GetFullPathNameW($0, 0, nil, nil)

            var szPath = Array<WCHAR>(repeating: 0, count: Int(dwLength + 1))
            dwLength = GetFullPathNameW($0, DWORD(szPath.count), &szPath, nil)
            guard dwLength > 0 && dwLength <= szPath.count else {
              throw _NSErrorWithWindowsError(GetLastError(), reading: true, paths: [path])
            }

            return String(decodingCString: szPath, as: UTF16.self)
          }
        }
        defer { CloseHandle(hFile) }

        let dwLength: DWORD = GetFinalPathNameByHandleW(hFile, nil, 0, DWORD(FILE_NAME_NORMALIZED))
        var szPath: [WCHAR] = Array<WCHAR>(repeating: 0, count: Int(dwLength + 1))

        GetFinalPathNameByHandleW(hFile, &szPath, dwLength, DWORD(FILE_NAME_NORMALIZED))
        return String(decodingCString: &szPath, as: UTF16.self)
    }

    internal func _copyRegularFile(atPath srcPath: String, toPath dstPath: String, variant: String = "Copy") throws {
      try FileManager.default._fileSystemRepresentation(withPath: srcPath, andPath: dstPath) {
        if !CopyFileW($0, $1, false) {
          throw _NSErrorWithWindowsError(GetLastError(), reading: true, paths: [srcPath, dstPath])
        }
      }
    }

    internal func _copySymlink(atPath srcPath: String, toPath dstPath: String, variant: String = "Copy") throws {
        let faAttributes: WIN32_FILE_ATTRIBUTE_DATA = try windowsFileAttributes(atPath: srcPath)
        guard faAttributes.dwFileAttributes & DWORD(FILE_ATTRIBUTE_REPARSE_POINT) == DWORD(FILE_ATTRIBUTE_REPARSE_POINT) else {
            throw _NSErrorWithErrno(EINVAL, reading: true, path: srcPath, extraUserInfo: extraErrorInfo(srcPath: srcPath, dstPath: dstPath, userVariant: variant))
        }

        let destination = try destinationOfSymbolicLink(atPath: srcPath)
        let isDir = try windowsFileAttributes(atPath: srcPath).dwFileAttributes & DWORD(FILE_ATTRIBUTE_DIRECTORY) == DWORD(FILE_ATTRIBUTE_DIRECTORY)
        if fileExists(atPath: dstPath) {
            try removeItem(atPath: dstPath)
        }
        try _createSymbolicLink(atPath: dstPath, withDestinationPath: destination, isDirectory: isDir)
    }

    internal func _copyOrLinkDirectoryHelper(atPath srcPath: String, toPath dstPath: String, variant: String = "Copy", _ body: (String, String, FileAttributeType) throws -> ()) throws {
        let faAttributes = try windowsFileAttributes(atPath: srcPath)

        var fileType = FileAttributeType(attributes: faAttributes, atPath: srcPath)
        if fileType == .typeDirectory {
          try createDirectory(atPath: dstPath, withIntermediateDirectories: false, attributes: nil)
          guard let enumerator = enumerator(atPath: srcPath) else {
            throw _NSErrorWithErrno(ENOENT, reading: true, path: srcPath)
          }

          while let item = enumerator.nextObject() as? String {
            let src = joinPath(prefix: srcPath, suffix: item)
            let dst = joinPath(prefix: dstPath, suffix: item)

            let faAttributes = try windowsFileAttributes(atPath: src)
            fileType = FileAttributeType(attributes: faAttributes, atPath: srcPath)
            if fileType == .typeDirectory {
              try createDirectory(atPath: dst, withIntermediateDirectories: false, attributes: nil)
            } else {
              try body(src, dst, fileType)
            }
          }
        } else {
          try body(srcPath, dstPath, fileType)
        }
    }

    internal func _moveItem(atPath srcPath: String, toPath dstPath: String, isURL: Bool) throws {
        guard shouldMoveItemAtPath(srcPath, toPath: dstPath, isURL: isURL) else {
            return
        }

        guard !self.fileExists(atPath: dstPath) else {
            throw NSError(domain: NSCocoaErrorDomain, code: CocoaError.fileWriteFileExists.rawValue, userInfo: [NSFilePathErrorKey : NSString(dstPath)])
        }

        try FileManager.default._fileSystemRepresentation(withPath: srcPath, andPath: dstPath) {
          if !MoveFileExW($0, $1, DWORD(MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH)) {
            throw _NSErrorWithWindowsError(GetLastError(), reading: false, paths: [srcPath, dstPath])
          }
        }
    }

    internal func _linkItem(atPath srcPath: String, toPath dstPath: String, isURL: Bool) throws {
        try _copyOrLinkDirectoryHelper(atPath: srcPath, toPath: dstPath) { (srcPath, dstPath, fileType) in
            guard shouldLinkItemAtPath(srcPath, toPath: dstPath, isURL: isURL) else {
                return
            }

            do {
                switch fileType {
                case .typeRegular:
                    try FileManager.default._fileSystemRepresentation(withPath: srcPath, andPath: dstPath) {
                      if !CreateHardLinkW($1, $0, nil) {
                        throw _NSErrorWithWindowsError(GetLastError(), reading: false, paths: [srcPath, dstPath])
                      }
                    }
                case .typeSymbolicLink:
                    try _copySymlink(atPath: srcPath, toPath: dstPath)
                default:
                    break
                }
            } catch {
                if !shouldProceedAfterError(error, linkingItemAtPath: srcPath, toPath: dstPath, isURL: isURL) {
                    throw error
                }
            }
        }
    }

    internal func _removeItem(atPath path: String, isURL: Bool, alreadyConfirmed: Bool = false) throws {
        guard alreadyConfirmed || shouldRemoveItemAtPath(path, isURL: isURL) else {
            return
        }

        let faAttributes: WIN32_FILE_ATTRIBUTE_DATA
        do {
            faAttributes = try windowsFileAttributes(atPath: path)
        } catch {
            // removeItem on POSIX throws fileNoSuchFile rather than
            // fileReadNoSuchFile that windowsFileAttributes will
            // throw if it doesn't find the file.
            if (error as NSError).code == CocoaError.fileReadNoSuchFile.rawValue {
                throw _NSErrorWithWindowsError(GetLastError(), reading: false, paths: [path])
            } else {
                throw error
            }
        }

        if faAttributes.dwFileAttributes & DWORD(FILE_ATTRIBUTE_READONLY) == FILE_ATTRIBUTE_READONLY {
          if try !FileManager.default._fileSystemRepresentation(withPath: path, {
            SetFileAttributesW($0, faAttributes.dwFileAttributes & ~DWORD(FILE_ATTRIBUTE_READONLY))
          }) {
            throw _NSErrorWithWindowsError(GetLastError(), reading: false, paths: [path])
          }
        }

        if faAttributes.dwFileAttributes & DWORD(FILE_ATTRIBUTE_DIRECTORY) == 0 {
          if try !FileManager.default._fileSystemRepresentation(withPath: path, DeleteFileW) {
            throw _NSErrorWithWindowsError(GetLastError(), reading: false, paths: [path])
          }
          return
        }

        var dirStack = [path]
        var itemPath = ""
        while let currentDir = dirStack.popLast() {
            do {
                itemPath = currentDir
                guard alreadyConfirmed || shouldRemoveItemAtPath(itemPath, isURL: isURL) else {
                    continue
                }

                if try FileManager.default._fileSystemRepresentation(withPath: itemPath, RemoveDirectoryW) {
                  continue
                }
                guard GetLastError() == ERROR_DIR_NOT_EMPTY else {
                    throw _NSErrorWithWindowsError(GetLastError(), reading: false, paths: [itemPath])
                }
                dirStack.append(itemPath)
                var ffd: WIN32_FIND_DATAW = WIN32_FIND_DATAW()
                let capacity = MemoryLayout.size(ofValue: ffd.cFileName)

                let handle: HANDLE = try FileManager.default._fileSystemRepresentation(withPath: itemPath + "\\*") {
                  FindFirstFileW($0, &ffd)
                }
                if handle == INVALID_HANDLE_VALUE {
                  throw _NSErrorWithWindowsError(GetLastError(), reading: false, paths: [itemPath])
                }
                defer { FindClose(handle) }

                repeat {
                    let file = withUnsafePointer(to: &ffd.cFileName) {
                      $0.withMemoryRebound(to: WCHAR.self, capacity: capacity) {
                        String(decodingCString: $0, as: UTF16.self)
                      }
                    }

                    itemPath = "\(currentDir)\\\(file)"
                    if ffd.dwFileAttributes & DWORD(FILE_ATTRIBUTE_READONLY) == FILE_ATTRIBUTE_READONLY {
                      if try !FileManager.default._fileSystemRepresentation(withPath: itemPath, {
                        SetFileAttributesW($0, ffd.dwFileAttributes & ~DWORD(FILE_ATTRIBUTE_READONLY))
                      }) {
                        throw _NSErrorWithWindowsError(GetLastError(), reading: false, paths: [file])
                      }
                    }

                    if (ffd.dwFileAttributes & DWORD(FILE_ATTRIBUTE_DIRECTORY) != 0) {
                        if file != "." && file != ".." {
                            dirStack.append(itemPath)
                        }
                    } else {
                        guard alreadyConfirmed || shouldRemoveItemAtPath(itemPath, isURL: isURL) else {
                            continue
                        }
                        if try !FileManager.default._fileSystemRepresentation(withPath: itemPath, DeleteFileW) {
                          throw _NSErrorWithWindowsError(GetLastError(), reading: false, paths: [file])
                        }
                    }
                } while FindNextFileW(handle, &ffd)
            } catch {
                if !shouldProceedAfterError(error, removingItemAtPath: itemPath, isURL: isURL) {
                    throw error
                }
            }
        }
    }

    internal func _currentDirectoryPath() -> String {
        let dwLength: DWORD = GetCurrentDirectoryW(0, nil)
        var szDirectory: [WCHAR] = Array<WCHAR>(repeating: 0, count: Int(dwLength + 1))

        GetCurrentDirectoryW(dwLength, &szDirectory)
        return String(decodingCString: &szDirectory, as: UTF16.self).standardizingPath
    }

    @discardableResult
    internal func _changeCurrentDirectoryPath(_ path: String) -> Bool {
        return (try? FileManager.default._fileSystemRepresentation(withPath: path) {
          SetCurrentDirectoryW($0)
        }) ?? false
    }

    internal func _fileExists(atPath path: String, isDirectory: UnsafeMutablePointer<ObjCBool>?) -> Bool {
        var faAttributes: WIN32_FILE_ATTRIBUTE_DATA = WIN32_FILE_ATTRIBUTE_DATA()
        do { faAttributes = try windowsFileAttributes(atPath: path) } catch { return false }
        if faAttributes.dwFileAttributes & DWORD(FILE_ATTRIBUTE_REPARSE_POINT) == DWORD(FILE_ATTRIBUTE_REPARSE_POINT) {
          let handle: HANDLE = (try? FileManager.default._fileSystemRepresentation(withPath: path) {
            CreateFileW($0, /* dwDesiredAccess= */ DWORD(0),
                        DWORD(FILE_SHARE_READ), /* lpSecurityAttributes= */ nil,
                        DWORD(OPEN_EXISTING),
                        DWORD(FILE_FLAG_BACKUP_SEMANTICS), /* hTemplateFile= */ nil)
          }) ?? INVALID_HANDLE_VALUE
          if handle == INVALID_HANDLE_VALUE { return false }
          defer { CloseHandle(handle) }

          if let isDirectory = isDirectory {
            var info: BY_HANDLE_FILE_INFORMATION = BY_HANDLE_FILE_INFORMATION()
            GetFileInformationByHandle(handle, &info)
            isDirectory.pointee = ObjCBool(info.dwFileAttributes & DWORD(FILE_ATTRIBUTE_DIRECTORY) == DWORD(FILE_ATTRIBUTE_DIRECTORY))
          }
        } else {
          if let isDirectory = isDirectory {
            isDirectory.pointee = ObjCBool(faAttributes.dwFileAttributes & DWORD(FILE_ATTRIBUTE_DIRECTORY) == DWORD(FILE_ATTRIBUTE_DIRECTORY))
          }
        }
        return true
    }


    internal func _isReadableFile(atPath path: String) -> Bool {
        do { let _ = try windowsFileAttributes(atPath: path) } catch { return false }
        return true
    }

    internal func _isWritableFile(atPath path: String) -> Bool {
        guard let faAttributes: WIN32_FILE_ATTRIBUTE_DATA = try? windowsFileAttributes(atPath: path) else { return false }
        return faAttributes.dwFileAttributes & DWORD(FILE_ATTRIBUTE_READONLY) != DWORD(FILE_ATTRIBUTE_READONLY)
    }

    internal func _isExecutableFile(atPath path: String) -> Bool {
        var binaryType = DWORD(0)
        return path.withCString(encodedAs: UTF16.self) {
            GetBinaryTypeW($0, &binaryType)
        }
    }

    internal func _isDeletableFile(atPath path: String) -> Bool {
        guard path != "" else { return true }

        // Get the parent directory of supplied path
        let parent = path._nsObject.deletingLastPathComponent
        var faAttributes: WIN32_FILE_ATTRIBUTE_DATA = WIN32_FILE_ATTRIBUTE_DATA()
        do { faAttributes = try windowsFileAttributes(atPath: parent) } catch { return false }
        if faAttributes.dwFileAttributes & DWORD(FILE_ATTRIBUTE_READONLY) == DWORD(FILE_ATTRIBUTE_READONLY) {
            return false
        }

        do { faAttributes = try windowsFileAttributes(atPath: path) } catch { return false }
        if faAttributes.dwFileAttributes & DWORD(FILE_ATTRIBUTE_READONLY) == DWORD(FILE_ATTRIBUTE_READONLY) {
            return false
        }

        return true
    }

    internal func _lstatFile(atPath path: String, withFileSystemRepresentation fsRep: UnsafePointer<NativeFSRCharType>? = nil) throws -> stat {
        let (stbuf, _) = try _statxFile(atPath: path, withFileSystemRepresentation: fsRep)
        return stbuf
    }

    // FIXME(compnerd) the UInt64 should be UInt128 to uniquely identify the file across volumes
    internal func _statxFile(atPath path: String, withFileSystemRepresentation fsRep: UnsafePointer<NativeFSRCharType>? = nil) throws -> (stat, UInt64) {
        let _fsRep: UnsafePointer<NativeFSRCharType>
        if fsRep == nil {
            _fsRep = try __fileSystemRepresentation(withPath: path)
        } else {
            _fsRep = fsRep!
        }

        defer {
            if fsRep == nil { _fsRep.deallocate() }
        }

        var statInfo = stat()
        let handle =
            CreateFileW(_fsRep, /*dwDesiredAccess=*/DWORD(0),
                        DWORD(FILE_SHARE_READ), /*lpSecurityAttributes=*/nil,
                        DWORD(OPEN_EXISTING),
                        DWORD(FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS),
                        /*hTemplateFile=*/nil)
        if handle == INVALID_HANDLE_VALUE {
            throw _NSErrorWithWindowsError(GetLastError(), reading: false, paths: [path])
        }
        defer { CloseHandle(handle) }

        var info: BY_HANDLE_FILE_INFORMATION = BY_HANDLE_FILE_INFORMATION()
        GetFileInformationByHandle(handle, &info)

        // Group id is always 0 on Windows
        statInfo.st_gid = 0
        statInfo.st_atime = info.ftLastAccessTime.time_t
        statInfo.st_ctime = info.ftCreationTime.time_t
        statInfo.st_dev = _dev_t(info.dwVolumeSerialNumber)
        // The inode, and therefore st_ino, has no meaning in the FAT, HPFS, or
        // NTFS file systems. -- docs.microsoft.com
        statInfo.st_ino = 0
        statInfo.st_rdev = _dev_t(info.dwVolumeSerialNumber)

        let isReparsePoint = info.dwFileAttributes & DWORD(FILE_ATTRIBUTE_REPARSE_POINT) != 0
        let isDir = info.dwFileAttributes & DWORD(FILE_ATTRIBUTE_DIRECTORY) != 0
        let fileMode = isDir ? _S_IFDIR : _S_IFREG
        // On a symlink to a directory, Windows sets both the REPARSE_POINT and
        // DIRECTORY attributes. Since Windows doesn't provide S_IFLNK and we
        // want unix style "symlinks to directories are not directories
        // themselves, we say symlinks are regular files
        statInfo.st_mode = UInt16(isReparsePoint ? _S_IFREG : fileMode)
        let isReadOnly = info.dwFileAttributes & DWORD(FILE_ATTRIBUTE_READONLY) != 0
        statInfo.st_mode |= UInt16(isReadOnly ? _S_IREAD : (_S_IREAD | _S_IWRITE))
        statInfo.st_mode |= UInt16(_S_IEXEC)

        statInfo.st_mtime = info.ftLastWriteTime.time_t
        statInfo.st_nlink = Int16(info.nNumberOfLinks)
        guard info.nFileSizeHigh == 0 else {
            throw _NSErrorWithErrno(EOVERFLOW, reading: true, path: path)
        }
        statInfo.st_size = _off_t(info.nFileSizeLow)
        // Uid is always 0 on Windows systems
        statInfo.st_uid = 0

        return (statInfo, UInt64(info.nFileIndexHigh << 32) | UInt64(info.nFileIndexLow))
    }

    internal func _contentsEqual(atPath path1: String, andPath path2: String) -> Bool {
        let path1Handle: HANDLE = (try? FileManager.default._fileSystemRepresentation(withPath: path1) {
          CreateFileW($0, GENERIC_READ,
                      DWORD(FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE),
                      nil, DWORD(OPEN_EXISTING),
                      DWORD(FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS),
                      nil)
        }) ?? INVALID_HANDLE_VALUE
        if path1Handle == INVALID_HANDLE_VALUE { return false }
        defer { CloseHandle(path1Handle) }

        let path2Handle: HANDLE = (try? FileManager.default._fileSystemRepresentation(withPath: path2) {
          CreateFileW($0, GENERIC_READ,
                      DWORD(FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE),
                      nil, DWORD(OPEN_EXISTING),
                      DWORD(FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS),
                      nil)
        }) ?? INVALID_HANDLE_VALUE
        if path2Handle == INVALID_HANDLE_VALUE { return false }
        defer { CloseHandle(path2Handle) }

        let file1Type = GetFileType(path1Handle)
        guard GetLastError() == NO_ERROR else {
            return false
        }
        let file2Type = GetFileType(path2Handle)
        guard GetLastError() == NO_ERROR else {
            return false
        }

        guard file1Type == FILE_TYPE_DISK, file2Type == FILE_TYPE_DISK else {
            return false
        }

        var path1FileInfo = BY_HANDLE_FILE_INFORMATION()
        var path2FileInfo = BY_HANDLE_FILE_INFORMATION()
        guard GetFileInformationByHandle(path1Handle, &path1FileInfo),
              GetFileInformationByHandle(path2Handle, &path2FileInfo) else {
            return false
        }

        // If both paths point to the same volume/filenumber or they are both zero length
        // then they are considered equal
        if path1FileInfo.nFileIndexHigh == path2FileInfo.nFileIndexHigh
              && path1FileInfo.nFileIndexLow == path2FileInfo.nFileIndexLow
              && path1FileInfo.dwVolumeSerialNumber == path2FileInfo.dwVolumeSerialNumber {
            return true
        }

        let path1Attrs = path1FileInfo.dwFileAttributes
        let path2Attrs = path2FileInfo.dwFileAttributes
        if path1Attrs & DWORD(FILE_ATTRIBUTE_REPARSE_POINT) == FILE_ATTRIBUTE_REPARSE_POINT
             || path2Attrs & DWORD(FILE_ATTRIBUTE_REPARSE_POINT) == FILE_ATTRIBUTE_REPARSE_POINT {
            guard path1Attrs & DWORD(FILE_ATTRIBUTE_REPARSE_POINT) == FILE_ATTRIBUTE_REPARSE_POINT
                    && path2Attrs & DWORD(FILE_ATTRIBUTE_REPARSE_POINT) == FILE_ATTRIBUTE_REPARSE_POINT else {
                return false
            }
            guard let pathDest1 = try? _destinationOfSymbolicLink(atPath: path1),
                  let pathDest2 = try? _destinationOfSymbolicLink(atPath: path2) else {
                return false
            }
            return pathDest1 == pathDest2
        } else if DWORD(FILE_ATTRIBUTE_DIRECTORY) & path1Attrs == DWORD(FILE_ATTRIBUTE_DIRECTORY)
                    || DWORD(FILE_ATTRIBUTE_DIRECTORY) & path2Attrs == DWORD(FILE_ATTRIBUTE_DIRECTORY) {
            guard DWORD(FILE_ATTRIBUTE_DIRECTORY) & path1Attrs == DWORD(FILE_ATTRIBUTE_DIRECTORY)
                    && DWORD(FILE_ATTRIBUTE_DIRECTORY) & path2Attrs == FILE_ATTRIBUTE_DIRECTORY else {
                return false
            }
            return _compareDirectories(atPath: path1, andPath: path2)
        } else {
            if path1FileInfo.nFileSizeHigh == 0 && path1FileInfo.nFileSizeLow == 0
              && path2FileInfo.nFileSizeHigh == 0 && path2FileInfo.nFileSizeLow == 0 {
                return true
            }

            return try! FileManager.default._fileSystemRepresentation(withPath: path1, andPath: path2) {
              _compareFiles(withFileSystemRepresentation: $0,
                            andFileSystemRepresentation: $1,
                            size: (Int64(path1FileInfo.nFileSizeHigh) << 32) | Int64(path1FileInfo.nFileSizeLow),
                            bufSize: 0x1000)
            }
        }
    }

    internal func _appendSymlinkDestination(_ dest: String, toPath: String) -> String {
        if dest.isAbsolutePath { return dest }
        let temp = toPath._bridgeToObjectiveC().deletingLastPathComponent
        return temp._bridgeToObjectiveC().appendingPathComponent(dest)
    }

    internal func _updateTimes(atPath path: String,
                               withFileSystemRepresentation fsr: UnsafePointer<NativeFSRCharType>,
                               creationTime: Date? = nil,
                               accessTime: Date? = nil,
                               modificationTime: Date? = nil) throws {
      let stat = try _lstatFile(atPath: path, withFileSystemRepresentation: fsr)

      var atime: FILETIME =
          FILETIME(from: time_t((accessTime ?? stat.lastAccessDate).timeIntervalSince1970))
      var mtime: FILETIME =
          FILETIME(from: time_t((modificationTime ?? stat.lastModificationDate).timeIntervalSince1970))

      let hFile: HANDLE =
        CreateFileW(fsr, DWORD(GENERIC_WRITE), DWORD(FILE_SHARE_WRITE),
                    nil, DWORD(OPEN_EXISTING), 0, nil)
      if hFile == INVALID_HANDLE_VALUE {
          throw _NSErrorWithWindowsError(GetLastError(), reading: true, paths: [path])
      }
      defer { CloseHandle(hFile) }

      if !SetFileTime(hFile, nil, &atime, &mtime) {
          throw _NSErrorWithWindowsError(GetLastError(), reading: false, paths: [path])
      }

    }

    internal class NSURLDirectoryEnumerator : DirectoryEnumerator {
        var _options : FileManager.DirectoryEnumerationOptions
        var _errorHandler : ((URL, Error) -> Bool)?
        var _stack: [URL]
        var _lastReturned: URL
        var _rootDepth : Int

        init(url: URL, options: FileManager.DirectoryEnumerationOptions, errorHandler: (/* @escaping */ (URL, Error) -> Bool)?) {
            _options = options
            _errorHandler = errorHandler
            _stack = []
            _rootDepth = url.pathComponents.count
            _lastReturned = url
        }

        override func nextObject() -> Any? {
            func firstValidItem() -> URL? {
                while let url = _stack.popLast() {
                    if !FileManager.default.fileExists(atPath: url.path) {
                      if let handler = _errorHandler {
                        if !handler(url, _NSErrorWithWindowsError(GetLastError(), reading: true, paths: [url.path])) {
                          return nil
                        }
                      } else {
                        return nil
                      }
                    }
                    _lastReturned = url
                    return _lastReturned
                }
                return nil
            }

            // If we most recently returned a directory, decend into it
            guard let attrs = try? FileManager.default.windowsFileAttributes(atPath: _lastReturned.path) else {
                guard let handler = _errorHandler,
                    handler(_lastReturned, _NSErrorWithWindowsError(GetLastError(), reading: true, paths: [_lastReturned.path]))
                else { return nil }
                return firstValidItem()
            }

            let isDir = attrs.dwFileAttributes & DWORD(FILE_ATTRIBUTE_DIRECTORY) == DWORD(FILE_ATTRIBUTE_DIRECTORY) &&
                attrs.dwFileAttributes & DWORD(FILE_ATTRIBUTE_REPARSE_POINT) == 0
            if isDir && (level == 0 || !_options.contains(.skipsSubdirectoryDescendants)) {
                var ffd = WIN32_FIND_DATAW()
                let capacity = MemoryLayout.size(ofValue: ffd.cFileName)

                let handle = (try? FileManager.default._fileSystemRepresentation(withPath: _lastReturned.path + "\\*") {
                  FindFirstFileW($0, &ffd)
                }) ?? INVALID_HANDLE_VALUE
                if handle == INVALID_HANDLE_VALUE { return firstValidItem() }
                defer { FindClose(handle) }

                repeat {
                    let file = withUnsafePointer(to: &ffd.cFileName) {
                      $0.withMemoryRebound(to: WCHAR.self, capacity: capacity) {
                        String(decodingCString: $0, as: UTF16.self)
                      }
                    }
                    if file == "." || file == ".." { continue }
                    if _options.contains(.skipsHiddenFiles) &&
                       ffd.dwFileAttributes & DWORD(FILE_ATTRIBUTE_HIDDEN) == DWORD(FILE_ATTRIBUTE_HIDDEN) {
                      continue
                    }
                    _stack.append(URL(fileURLWithPath: file, relativeTo: _lastReturned))
                } while FindNextFileW(handle, &ffd)
            }
            return firstValidItem()
        }

        override var level: Int {
            return _lastReturned.pathComponents.count - _rootDepth
        }

        override func skipDescendants() {
            _options.insert(.skipsSubdirectoryDescendants)
        }

        override var directoryAttributes : [FileAttributeKey : Any]? {
            return nil
        }

        override var fileAttributes: [FileAttributeKey : Any]? {
            return nil
        }
    }
}

extension FileManager.NSPathDirectoryEnumerator {
    internal func _nextObject() -> Any? {
        guard let url = innerEnumerator.nextObject() as? URL else { return nil }

        var relativePath: [WCHAR] = Array<WCHAR>(repeating: 0, count: Int(MAX_PATH))

        guard baseURL._withUnsafeWideFileSystemRepresentation({ baseUrlFsr in
            url._withUnsafeWideFileSystemRepresentation { urlFsr in
                let fromAttrs = GetFileAttributesW(baseUrlFsr)
                let toAttrs = GetFileAttributesW(urlFsr)
                guard fromAttrs != INVALID_FILE_ATTRIBUTES, toAttrs != INVALID_FILE_ATTRIBUTES else {
                    return false
                }
                return PathRelativePathToW(&relativePath, baseUrlFsr, fromAttrs, urlFsr, toAttrs)
            }
        }) else { return nil }

        let path = String(decodingCString: &relativePath, as: UTF16.self)
        // Drop the leading ".\" from the path
        _currentItemPath = String(path.dropFirst(2))
        return _currentItemPath
    }

}

#endif
