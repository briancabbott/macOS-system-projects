/*
 This source file is part of the Swift.org open source project

 Copyright (c) 2014 - 2022 Apple Inc. and the Swift project authors
 Licensed under Apache License v2.0 with Runtime Library Exception

 See http://swift.org/LICENSE.txt for license information
 See http://swift.org/CONTRIBUTORS.txt for Swift project authors
*/

import XCTest

import TSCBasic
import TSCTestSupport
import TSCLibc

class FileSystemTests: XCTestCase {

    // MARK: LocalFS Tests

    func testLocalBasics() throws {
        let fs = TSCBasic.localFileSystem
        try! withTemporaryFile { file in
            try! withTemporaryDirectory(removeTreeOnDeinit: true) { tempDirPath in
                // exists()
                XCTAssert(fs.exists(AbsolutePath("/")))
                XCTAssert(!fs.exists(AbsolutePath("/does-not-exist")))

                // isFile()
                XCTAssertTrue(fs.exists(file.path))
                XCTAssertTrue(fs.isFile(file.path))
                XCTAssertEqual(try fs.getFileInfo(file.path).fileType, .typeRegular)
                XCTAssertFalse(fs.isDirectory(file.path))
                XCTAssertFalse(fs.isFile(AbsolutePath("/does-not-exist")))
                XCTAssertFalse(fs.isSymlink(AbsolutePath("/does-not-exist")))
                XCTAssertThrowsError(try fs.getFileInfo(AbsolutePath("/does-not-exist")))

                // isSymlink()
                let sym = tempDirPath.appending(component: "hello")
                try! fs.createSymbolicLink(sym, pointingAt: file.path, relative: false)
                XCTAssertTrue(fs.isSymlink(sym))
                XCTAssertTrue(fs.isFile(sym))
                XCTAssertEqual(try fs.getFileInfo(sym).fileType, .typeSymbolicLink)
                XCTAssertFalse(fs.isDirectory(sym))

                // isExecutableFile
                let executable = tempDirPath.appending(component: "exec-foo")
                let executableSym = tempDirPath.appending(component: "exec-sym")
                try! fs.createSymbolicLink(executableSym, pointingAt: executable, relative: false)
                let stream = BufferedOutputByteStream()
                stream <<< """
                    #!/bin/sh
                    set -e
                    exit

                    """
                try! fs.writeFileContents(executable, bytes: stream.bytes)
                try! Process.checkNonZeroExit(args: "chmod", "+x", executable.pathString)
                XCTAssertTrue(fs.isExecutableFile(executable))
                XCTAssertTrue(fs.isExecutableFile(executableSym))
                XCTAssertTrue(fs.isSymlink(executableSym))
                XCTAssertFalse(fs.isExecutableFile(sym))
                XCTAssertFalse(fs.isExecutableFile(file.path))
                XCTAssertFalse(fs.isExecutableFile(AbsolutePath("/does-not-exist")))
                XCTAssertFalse(fs.isExecutableFile(AbsolutePath("/")))

                // isDirectory()
                XCTAssert(fs.isDirectory(AbsolutePath("/")))
                XCTAssert(!fs.isDirectory(AbsolutePath("/does-not-exist")))

                // getDirectoryContents()
                do {
                    _ = try fs.getDirectoryContents(AbsolutePath("/does-not-exist"))
                    XCTFail("Unexpected success")
                } catch {
                    XCTAssertEqual(error.localizedDescription, "The folder “does-not-exist” doesn’t exist.")
                }

                let thisDirectoryContents = try! fs.getDirectoryContents(AbsolutePath(#file).parentDirectory)
                XCTAssertTrue(!thisDirectoryContents.contains(where: { $0 == "." }))
                XCTAssertTrue(!thisDirectoryContents.contains(where: { $0 == ".." }))
                XCTAssertTrue(thisDirectoryContents.contains(where: { $0 == AbsolutePath(#file).basename }))
            }
        }
    }

    func testResolvingSymlinks() {
        // Make sure the root path resolves to itself.
        XCTAssertEqual(resolveSymlinks(AbsolutePath.root), AbsolutePath.root)

        // For the rest of the tests we'll need a temporary directory.
        try! withTemporaryDirectory(removeTreeOnDeinit: true) { path in
            // FIXME: it would be better to not need to resolve symbolic links, but we end up relying on /tmp -> /private/tmp.
            let tmpDirPath = resolveSymlinks(path)

            // Create a symbolic link and directory.
            let slnkPath = tmpDirPath.appending(component: "slnk")
            let fldrPath = tmpDirPath.appending(component: "fldr")

            // Create a symbolic link pointing at the (so far non-existent) directory.
            try! localFileSystem.createSymbolicLink(slnkPath, pointingAt: fldrPath, relative: true)

            // Resolving the symlink should not yet change anything.
            XCTAssertEqual(resolveSymlinks(slnkPath), slnkPath)

            // Create a directory to be the referent of the symbolic link.
            try! makeDirectories(fldrPath)

            // Resolving the symlink should now point at the directory.
            XCTAssertEqual(resolveSymlinks(slnkPath), fldrPath)

            // Resolving the directory should still not change anything.
            XCTAssertEqual(resolveSymlinks(fldrPath), fldrPath)
        }
    }

    func testSymlinksNotWalked() {
        try! withTemporaryDirectory(removeTreeOnDeinit: true) { path in
            // FIXME: it would be better to not need to resolve symbolic links, but we end up relying on /tmp -> /private/tmp.
            let tmpDirPath = resolveSymlinks(path)

            try! makeDirectories(tmpDirPath.appending(component: "foo"))
            try! makeDirectories(tmpDirPath.appending(components: "bar", "baz", "goo"))
            try! localFileSystem.createSymbolicLink(tmpDirPath.appending(components: "foo", "symlink"), pointingAt: tmpDirPath.appending(component: "bar"), relative: true)

            XCTAssertTrue(localFileSystem.isSymlink(tmpDirPath.appending(components: "foo", "symlink")))
            XCTAssertEqual(resolveSymlinks(tmpDirPath.appending(components: "foo", "symlink")), tmpDirPath.appending(component: "bar"))
            XCTAssertTrue(localFileSystem.isDirectory(resolveSymlinks(tmpDirPath.appending(components: "foo", "symlink", "baz"))))

            let results = try! walk(tmpDirPath.appending(component: "foo")).map{ $0 }

            XCTAssertEqual(results, [tmpDirPath.appending(components: "foo", "symlink")])
        }
    }

    func testWalkingADirectorySymlinkResolvesOnce() {
        try! withTemporaryDirectory(removeTreeOnDeinit: true) { tmpDirPath in
            try! makeDirectories(tmpDirPath.appending(components: "foo", "bar"))
            try! makeDirectories(tmpDirPath.appending(components: "abc", "bar"))
            try! localFileSystem.createSymbolicLink(tmpDirPath.appending(component: "symlink"), pointingAt: tmpDirPath.appending(component: "foo"), relative: true)
            try! localFileSystem.createSymbolicLink(tmpDirPath.appending(components: "foo", "baz"), pointingAt: tmpDirPath.appending(component: "abc"), relative: true)

            XCTAssertTrue(localFileSystem.isSymlink(tmpDirPath.appending(component: "symlink")))

            let results = try! walk(tmpDirPath.appending(component: "symlink")).map{ $0 }.sorted()

            // we recurse a symlink to a directory, so this should work,
            // but `abc` should not show because `baz` is a symlink too
            // and that should *not* be followed

            XCTAssertEqual(results, [tmpDirPath.appending(components: "symlink", "bar"), tmpDirPath.appending(components: "symlink", "baz")])
        }
    }

    func testLocalExistsSymlink() throws {
        try testWithTemporaryDirectory { tmpdir in
            let fs = TSCBasic.localFileSystem

            let source = tmpdir.appending(component: "source")
            let target = tmpdir.appending(component: "target")
            try fs.writeFileContents(target, bytes: "source")

            // Source and target exist.

            try fs.createSymbolicLink(source, pointingAt: target, relative: false)
            XCTAssertEqual(fs.exists(source), true)
            XCTAssertEqual(fs.exists(source, followSymlink: true), true)
            XCTAssertEqual(fs.exists(source, followSymlink: false), true)

            // Source only exists.

            try fs.removeFileTree(target)
            XCTAssertEqual(fs.exists(source), false)
            XCTAssertEqual(fs.exists(source, followSymlink: true), false)
            XCTAssertEqual(fs.exists(source, followSymlink: false), true)

            // None exist.

            try fs.removeFileTree(source)
            XCTAssertEqual(fs.exists(source), false)
            XCTAssertEqual(fs.exists(source, followSymlink: true), false)
            XCTAssertEqual(fs.exists(source, followSymlink: false), false)
        }
    }

    func testLocalReadableWritable() throws {
        try testWithTemporaryDirectory { tmpdir in
            let fs = localFileSystem

            // directory

            do {
                let directory = tmpdir.appending(component: "directory")
                try fs.createDirectory(directory, recursive: true)

                // should be readable and writable by default
                XCTAssertTrue(fs.isReadable(directory))
                XCTAssertTrue(fs.isWritable(directory))

                // set to non-readable non-writable.
                _ = try Process.popen(args: "chmod", "-r-w", directory.pathString)
                XCTAssertFalse(fs.isReadable(directory))
                XCTAssertFalse(fs.isWritable(directory))

                // set to readable non-writable.
                _ = try Process.popen(args: "chmod", "+r-w", directory.pathString)
                XCTAssertTrue(fs.isReadable(directory))
                XCTAssertFalse(fs.isWritable(directory))

                // set to non-readable writable.
                _ = try Process.popen(args: "chmod", "-r+w", directory.pathString)
                XCTAssertFalse(fs.isReadable(directory))
                XCTAssertTrue(fs.isWritable(directory))

                // set to readable and writable.
                _ = try Process.popen(args: "chmod", "+r+w", directory.pathString)
                XCTAssertTrue(fs.isReadable(directory))
                XCTAssertTrue(fs.isWritable(directory))
            }

            // file

            do {
                let file = tmpdir.appending(component: "file")
                try fs.writeFileContents(file, bytes: "")

                // should be readable and writable by default
                XCTAssertTrue(fs.isReadable(file))
                XCTAssertTrue(fs.isWritable(file))

                // set to non-readable non-writable.
                _ = try Process.popen(args: "chmod", "-r-w", file.pathString)
                XCTAssertFalse(fs.isReadable(file))
                XCTAssertFalse(fs.isWritable(file))

                // set to readable non-writable.
                _ = try Process.popen(args: "chmod", "+r-w", file.pathString)
                XCTAssertTrue(fs.isReadable(file))
                XCTAssertFalse(fs.isWritable(file))

                // set to non-readable writable.
                _ = try Process.popen(args: "chmod", "-r+w", file.pathString)
                XCTAssertFalse(fs.isReadable(file))
                XCTAssertTrue(fs.isWritable(file))

                // set to readable and writable.
                _ = try Process.popen(args: "chmod", "+r+w", file.pathString)
                XCTAssertTrue(fs.isReadable(file))
                XCTAssertTrue(fs.isWritable(file))
            }
        }
    }

    func testLocalCreateDirectory() throws {
        let fs = TSCBasic.localFileSystem

        try withTemporaryDirectory(prefix: #function, removeTreeOnDeinit: true) { tmpDirPath in
            do {
                let testPath = tmpDirPath.appending(component: "new-dir")
                XCTAssert(!fs.exists(testPath))
                try fs.createDirectory(testPath)
                try fs.createDirectory(testPath)
                XCTAssert(fs.exists(testPath))
                XCTAssert(fs.isDirectory(testPath))
            }

            do {
                let testPath = tmpDirPath.appending(components: "another-new-dir", "with-a-subdir")
                XCTAssert(!fs.exists(testPath))
                try fs.createDirectory(testPath, recursive: true)
                XCTAssert(fs.exists(testPath))
                XCTAssert(fs.isDirectory(testPath))
            }
        }
    }

    func testLocalReadWriteFile() throws {
        let fs = TSCBasic.localFileSystem

        try withTemporaryDirectory(prefix: #function, removeTreeOnDeinit: true) { tmpDirPath in
            // Check read/write of a simple file.
            let testData = (0..<1000).map { $0.description }.joined(separator: ", ")
            let filePath = tmpDirPath.appending(component: "test-data.txt")
            try! fs.writeFileContents(filePath, bytes: ByteString(testData))
            XCTAssertTrue(fs.isFile(filePath))
            let data = try! fs.readFileContents(filePath)
            XCTAssertEqual(data, ByteString(testData))

            // Atomic writes
            let inMemoryFilePath = AbsolutePath("/file.text")
            XCTAssertNoThrow(try TSCBasic.InMemoryFileSystem(files: [:]).writeFileContents(inMemoryFilePath, bytes: ByteString(testData), atomically: true))
            XCTAssertNoThrow(try TSCBasic.InMemoryFileSystem(files: [:]).writeFileContents(inMemoryFilePath, bytes: ByteString(testData), atomically: false))
            // Local file system does support atomic writes, so it doesn't throw.
            let byteString = ByteString(testData)
            let filePath1 = tmpDirPath.appending(components: "test-data-1.txt")
            XCTAssertNoThrow(try fs.writeFileContents(filePath1, bytes: byteString, atomically: false))
            let read1 = try fs.readFileContents(filePath1)
            XCTAssertEqual(read1, byteString)

            // Test overwriting file non-atomically
            XCTAssertNoThrow(try fs.writeFileContents(filePath1, bytes: byteString, atomically: false))

            let filePath2 = tmpDirPath.appending(components: "test-data-2.txt")
            XCTAssertNoThrow(try fs.writeFileContents(filePath2, bytes: byteString, atomically: true))
            let read2 = try fs.readFileContents(filePath2)
            XCTAssertEqual(read2, byteString)

            // Test overwriting file atomically
            XCTAssertNoThrow(try fs.writeFileContents(filePath2, bytes: byteString, atomically: true))

            // Check overwrite of a file.
            try! fs.writeFileContents(filePath, bytes: "Hello, new world!")
            XCTAssertEqual(try! fs.readFileContents(filePath), "Hello, new world!")

            // Check read/write of a directory.
            XCTAssertThrows(FileSystemError(.ioError(code: TSCLibc.EPERM), filePath.parentDirectory)) {
                _ = try fs.readFileContents(filePath.parentDirectory)
            }
            XCTAssertThrows(FileSystemError(.isDirectory, filePath.parentDirectory)) {
                try fs.writeFileContents(filePath.parentDirectory, bytes: [])
            }
            XCTAssertEqual(try! fs.readFileContents(filePath), "Hello, new world!")

            // Check read/write against root.
            #if os(Android)
            let root = AbsolutePath("/system/")
            #else
            let root = AbsolutePath("/")
            #endif
            XCTAssertThrows(FileSystemError(.ioError(code: TSCLibc.EPERM), root)) {
                _ = try fs.readFileContents(root)

            }
            XCTAssertThrows(FileSystemError(.isDirectory, root)) {
                try fs.writeFileContents(root, bytes: [])
            }
            XCTAssert(fs.exists(filePath))

            // Check read/write into a non-directory.
            let notDirectoryPath = filePath.appending(component: "not-possible")
            XCTAssertThrows(FileSystemError(.notDirectory, notDirectoryPath)) {
                _ = try fs.readFileContents(notDirectoryPath)
            }
            XCTAssertThrows(FileSystemError(.notDirectory, notDirectoryPath)) {
                try fs.writeFileContents(filePath.appending(component: "not-possible"), bytes: [])
            }
            XCTAssert(fs.exists(filePath))

            // Check read/write into a missing directory.
            let missingDir = tmpDirPath.appending(components: "does", "not", "exist")
            XCTAssertThrows(FileSystemError(.noEntry, missingDir)) {
                _ = try fs.readFileContents(missingDir)
            }
            XCTAssertThrows(FileSystemError(.noEntry, missingDir)) {
                try fs.writeFileContents(missingDir, bytes: [])
            }
            XCTAssert(!fs.exists(missingDir))
        }
    }

    func testRemoveFileTree() throws {
        try testWithTemporaryDirectory { tmpdir in
            try removeFileTreeTester(fs: localFileSystem, basePath: tmpdir)
        }
    }

    func testCopyAndMoveItem() throws {
        let fs = TSCBasic.localFileSystem

        try testWithTemporaryDirectory { tmpdir in
            let source = tmpdir.appending(component: "source")
            let destination = tmpdir.appending(component: "destination")

            // Copy with no source

            XCTAssertThrows(FileSystemError(.noEntry, source)) {
                try fs.copy(from: source, to: destination)
            }
            XCTAssertThrows(FileSystemError(.noEntry, source)) {
                try fs.move(from: source, to: destination)
            }

            // Copy with a file at destination

            try fs.writeFileContents(source, bytes: "source1")
            try fs.writeFileContents(destination, bytes: "destination")

            XCTAssertThrows(FileSystemError(.alreadyExistsAtDestination, destination)) {
                try fs.copy(from: source, to: destination)
            }
            XCTAssertThrows(FileSystemError(.alreadyExistsAtDestination, destination)) {
                try fs.move(from: source, to: destination)
            }

            // Copy file

            try fs.removeFileTree(destination)

            XCTAssertNoThrow(try fs.copy(from: source, to: destination))
            XCTAssert(fs.exists(source))
            XCTAssertEqual(try fs.readFileContents(destination).cString, "source1")

            // Move file

            try fs.removeFileTree(destination)
            try fs.writeFileContents(source, bytes: "source2")

            XCTAssertNoThrow(try fs.move(from: source, to: destination))
            XCTAssert(!fs.exists(source))
            XCTAssertEqual(try fs.readFileContents(destination).cString, "source2")

            let sourceChild = source.appending(component: "child")
            let destinationChild = destination.appending(component: "child")

            // Copy directory

            try fs.createDirectory(source)
            try fs.writeFileContents(sourceChild, bytes: "source3")
            try fs.removeFileTree(destination)

            XCTAssertNoThrow(try fs.copy(from: source, to: destination))
            XCTAssertEqual(try fs.readFileContents(destinationChild).cString, "source3")

            // Move directory

            try fs.writeFileContents(sourceChild, bytes: "source4")
            try fs.removeFileTree(destination)

            XCTAssertNoThrow(try fs.move(from: source, to: destination))
            XCTAssert(!fs.exists(source))
            XCTAssertEqual(try fs.readFileContents(destinationChild).cString, "source4")

            // Copy to non-existant folder

            try fs.writeFileContents(source, bytes: "source3")
            try fs.removeFileTree(destination)

            XCTAssertThrowsError(try fs.copy(from: source, to: destinationChild))
            XCTAssertThrowsError(try fs.move(from: source, to: destinationChild))
        }
    }

    // MARK: InMemoryFileSystem Tests

    func testInMemoryBasics() throws {
        let fs = InMemoryFileSystem()
        let doesNotExist = AbsolutePath("/does-not-exist")

        // exists()
        XCTAssert(!fs.exists(doesNotExist))

        // isDirectory()
        XCTAssert(!fs.isDirectory(doesNotExist))

        // isFile()
        XCTAssert(!fs.isFile(doesNotExist))

        // isSymlink()
        XCTAssert(!fs.isSymlink(doesNotExist))

        // getDirectoryContents()
        XCTAssertThrows(FileSystemError(.noEntry, doesNotExist)) {
            _ = try fs.getDirectoryContents(doesNotExist)
        }

        // createDirectory()
        XCTAssert(!fs.isDirectory(AbsolutePath("/new-dir")))
        try fs.createDirectory(AbsolutePath("/new-dir/subdir"), recursive: true)
        XCTAssert(fs.isDirectory(AbsolutePath("/new-dir")))
        XCTAssert(fs.isDirectory(AbsolutePath("/new-dir/subdir")))
        XCTAssertEqual(try fs.getDirectoryContents(AbsolutePath("/")), ["new-dir"])
        XCTAssertEqual(try fs.getDirectoryContents(AbsolutePath("/new-dir")), ["subdir"])
    }

    func testInMemoryCreateDirectory() {
        let fs = InMemoryFileSystem()
        // Make sure root entry isn't created.
        try! fs.createDirectory(AbsolutePath("/"), recursive: true)
        let rootContents = try! fs.getDirectoryContents(.root)
        XCTAssertEqual(rootContents, [])

        let subdir = AbsolutePath("/new-dir/subdir")
        try! fs.createDirectory(subdir, recursive: true)
        XCTAssert(fs.isDirectory(subdir))

        // Check duplicate creation.
        try! fs.createDirectory(subdir, recursive: true)
        XCTAssert(fs.isDirectory(subdir))

        // Check non-recursive subdir creation.
        let subsubdir = subdir.appending(component: "new-subdir")
        XCTAssert(!fs.isDirectory(subsubdir))
        try! fs.createDirectory(subsubdir, recursive: false)
        XCTAssert(fs.isDirectory(subsubdir))

        // Check non-recursive failing subdir case.
        let veryNewDir = AbsolutePath("/very-new-dir")
        let newsubdir = veryNewDir.appending(component: "subdir")
        XCTAssert(!fs.isDirectory(newsubdir))
        XCTAssertThrows(FileSystemError(.noEntry, veryNewDir)) {
            try fs.createDirectory(newsubdir, recursive: false)
        }
        XCTAssert(!fs.isDirectory(newsubdir))

        // Check directory creation over a file.
        let filePath = AbsolutePath("/mach_kernel")
        try! fs.writeFileContents(filePath, bytes: [0xCD, 0x0D])
        XCTAssert(fs.exists(filePath) && !fs.isDirectory(filePath))
        XCTAssertThrows(FileSystemError(.notDirectory, filePath)) {
            try fs.createDirectory(filePath, recursive: true)
        }
        XCTAssertThrows(FileSystemError(.notDirectory, filePath)) {
            try fs.createDirectory(filePath.appending(component: "not-possible"), recursive: true)
        }
        XCTAssert(fs.exists(filePath) && !fs.isDirectory(filePath))
    }

    func testInMemoryCreateSymlink() throws {
        let fs = InMemoryFileSystem()
        let path = fs.homeDirectory
        try fs.createDirectory(path, recursive: true)

        let source = path.appending(component: "source")
        let target = path.appending(component: "target")
        try fs.writeFileContents(target, bytes: "source")

        // Source and target exist.

        try fs.createSymbolicLink(source, pointingAt: target, relative: false)
        XCTAssertEqual(fs.exists(source), true)
        XCTAssertEqual(fs.exists(source, followSymlink: true), true)
        XCTAssertEqual(fs.exists(source, followSymlink: false), true)

        // Source only exists.

        try fs.removeFileTree(target)
        XCTAssertEqual(fs.exists(source), false)
        XCTAssertEqual(fs.exists(source, followSymlink: true), false)
        XCTAssertEqual(fs.exists(source, followSymlink: false), true)

        // None exist.

        try fs.removeFileTree(source)
        XCTAssertEqual(fs.exists(source), false)
        XCTAssertEqual(fs.exists(source, followSymlink: true), false)
        XCTAssertEqual(fs.exists(source, followSymlink: false), false)
    }

    func testInMemoryReadWriteFile() {
        let fs = InMemoryFileSystem()
        try! fs.createDirectory(AbsolutePath("/new-dir/subdir"), recursive: true)

        // Check read/write of a simple file.
        let filePath = AbsolutePath("/new-dir/subdir").appending(component: "new-file.txt")
        XCTAssert(!fs.exists(filePath))
        XCTAssertFalse(fs.isFile(filePath))
        try! fs.writeFileContents(filePath, bytes: "Hello, world!")
        XCTAssert(fs.exists(filePath))
        XCTAssertTrue(fs.isFile(filePath))
        XCTAssertFalse(fs.isSymlink(filePath))
        XCTAssert(!fs.isDirectory(filePath))
        XCTAssertEqual(try! fs.readFileContents(filePath), "Hello, world!")

        // Check overwrite of a file.
        try! fs.writeFileContents(filePath, bytes: "Hello, new world!")
        XCTAssertEqual(try! fs.readFileContents(filePath), "Hello, new world!")

        // Check read/write of a directory.
        XCTAssertThrows(FileSystemError(.isDirectory, filePath.parentDirectory)) {
            _ = try fs.readFileContents(filePath.parentDirectory)
        }
        XCTAssertThrows(FileSystemError(.isDirectory, filePath.parentDirectory)) {
            try fs.writeFileContents(filePath.parentDirectory, bytes: [])
        }
        XCTAssertEqual(try! fs.readFileContents(filePath), "Hello, new world!")

        // Check read/write against root.
        let root = AbsolutePath("/")
        XCTAssertThrows(FileSystemError(.isDirectory, root)) {
            _ = try fs.readFileContents(root)
        }
        XCTAssertThrows(FileSystemError(.isDirectory, root)) {
            try fs.writeFileContents(root, bytes: [])
        }
        XCTAssert(fs.exists(filePath))
        XCTAssertTrue(fs.isFile(filePath))

        // Check read/write into a non-directory.
        let notDirectory = filePath.appending(component: "not-possible")
        XCTAssertThrows(FileSystemError(.notDirectory, filePath)) {
            _ = try fs.readFileContents(notDirectory)
        }
        XCTAssertThrows(FileSystemError(.notDirectory, filePath)) {
            try fs.writeFileContents(notDirectory, bytes: [])
        }
        XCTAssert(fs.exists(filePath))

        // Check read/write into a missing directory.
        let missingParent = AbsolutePath("/does/not")
        let missingFile = missingParent.appending(component: "exist")
        XCTAssertThrows(FileSystemError(.noEntry, missingFile)) {
            _ = try fs.readFileContents(missingFile)
        }
        XCTAssertThrows(FileSystemError(.noEntry, missingParent)) {
            try fs.writeFileContents(missingFile, bytes: [])
        }
        XCTAssert(!fs.exists(missingFile))
    }

    func testInMemoryFsCopy() throws {
        let fs = InMemoryFileSystem()
        try! fs.createDirectory(AbsolutePath("/new-dir/subdir"), recursive: true)
        let filePath = AbsolutePath("/new-dir/subdir").appending(component: "new-file.txt")
        try! fs.writeFileContents(filePath, bytes: "Hello, world!")
        XCTAssertEqual(try! fs.readFileContents(filePath), "Hello, world!")

        let copyFs = fs.copy()
        XCTAssertEqual(try! copyFs.readFileContents(filePath), "Hello, world!")
        try! copyFs.writeFileContents(filePath, bytes: "Hello, world 2!")

        XCTAssertEqual(try! fs.readFileContents(filePath), "Hello, world!")
        XCTAssertEqual(try! copyFs.readFileContents(filePath), "Hello, world 2!")
    }

    func testInMemRemoveFileTree() throws {
        let fs = InMemoryFileSystem() as FileSystem
        try removeFileTreeTester(fs: fs, basePath: .root)
    }

    func testInMemCopyAndMoveItem() throws {
        let fs = InMemoryFileSystem()
        let path = AbsolutePath("/tmp")
        try fs.createDirectory(path)
        let source = path.appending(component: "source")
        let destination = path.appending(component: "destination")

        // Copy with no source

        XCTAssertThrows(FileSystemError(.noEntry, source)) {
            try fs.copy(from: source, to: destination)
        }
        XCTAssertThrows(FileSystemError(.noEntry, source)) {
            try fs.move(from: source, to: destination)
        }

        // Copy with a file at destination

        try fs.writeFileContents(source, bytes: "source1")
        try fs.writeFileContents(destination, bytes: "destination")

        XCTAssertThrows(FileSystemError(.alreadyExistsAtDestination, destination)) {
            try fs.copy(from: source, to: destination)
        }
        XCTAssertThrows(FileSystemError(.alreadyExistsAtDestination, destination)) {
            try fs.move(from: source, to: destination)
        }

        // Copy file

        try fs.removeFileTree(destination)

        XCTAssertNoThrow(try fs.copy(from: source, to: destination))
        XCTAssert(fs.exists(source))
        XCTAssertEqual(try fs.readFileContents(destination).cString, "source1")

        // Move file

        try fs.removeFileTree(destination)
        try fs.writeFileContents(source, bytes: "source2")

        XCTAssertNoThrow(try fs.move(from: source, to: destination))
        XCTAssert(!fs.exists(source))
        XCTAssertEqual(try fs.readFileContents(destination).cString, "source2")

        let sourceChild = source.appending(component: "child")
        let destinationChild = destination.appending(component: "child")

        // Copy directory

        try fs.createDirectory(source)
        try fs.writeFileContents(sourceChild, bytes: "source3")
        try fs.removeFileTree(destination)

        XCTAssertNoThrow(try fs.copy(from: source, to: destination))
        XCTAssertEqual(try fs.readFileContents(destinationChild).cString, "source3")

        // Move directory

        try fs.writeFileContents(sourceChild, bytes: "source4")
        try fs.removeFileTree(destination)

        XCTAssertNoThrow(try fs.move(from: source, to: destination))
        XCTAssert(!fs.exists(source))
        XCTAssertEqual(try fs.readFileContents(destinationChild).cString, "source4")

        // Copy to non-existant folder

        try fs.writeFileContents(source, bytes: "source3")
        try fs.removeFileTree(destination)

        XCTAssertThrowsError(try fs.copy(from: source, to: destinationChild))
        XCTAssertThrowsError(try fs.move(from: source, to: destinationChild))
    }

    // MARK: RootedFileSystem Tests

    func testRootedFileSystem() throws {
        // Create the test file system.
        let baseFileSystem = InMemoryFileSystem() as FileSystem
        try baseFileSystem.createDirectory(AbsolutePath("/base/rootIsHere/subdir"), recursive: true)
        try baseFileSystem.writeFileContents(AbsolutePath("/base/rootIsHere/subdir/file"), bytes: "Hello, world!")

        // Create the rooted file system.
        let rerootedFileSystem = RerootedFileSystemView(baseFileSystem, rootedAt: AbsolutePath("/base/rootIsHere"))

        // Check that it has the appropriate view.
        XCTAssert(rerootedFileSystem.exists(AbsolutePath("/subdir")))
        XCTAssert(rerootedFileSystem.isDirectory(AbsolutePath("/subdir")))
        XCTAssert(rerootedFileSystem.exists(AbsolutePath("/subdir/file")))
        XCTAssertEqual(try rerootedFileSystem.readFileContents(AbsolutePath("/subdir/file")), "Hello, world!")

        // Check that mutations work appropriately.
        XCTAssert(!baseFileSystem.exists(AbsolutePath("/base/rootIsHere/subdir2")))
        try rerootedFileSystem.createDirectory(AbsolutePath("/subdir2"))
        XCTAssert(baseFileSystem.isDirectory(AbsolutePath("/base/rootIsHere/subdir2")))
    }

    func testRootedCreateSymlink() throws {
        // Create the test file system.
        let baseFileSystem = InMemoryFileSystem() as FileSystem
        try baseFileSystem.createDirectory(AbsolutePath("/base/rootIsHere/subdir"), recursive: true)

        // Create the rooted file system.
        let fs = RerootedFileSystemView(baseFileSystem, rootedAt: AbsolutePath("/base/rootIsHere"))

        let path = AbsolutePath("/test")
        try fs.createDirectory(path, recursive: true)

        let source = path.appending(component: "source")
        let target = path.appending(component: "target")
        try fs.writeFileContents(target, bytes: "source")

        // Source and target exist.

        try fs.createSymbolicLink(source, pointingAt: target, relative: false)
        XCTAssertEqual(fs.exists(source), true)
        XCTAssertEqual(fs.exists(source, followSymlink: true), true)
        XCTAssertEqual(fs.exists(source, followSymlink: false), true)

        // Source only exists.

        try fs.removeFileTree(target)
        XCTAssertEqual(fs.exists(source), false)
        XCTAssertEqual(fs.exists(source, followSymlink: true), false)
        XCTAssertEqual(fs.exists(source, followSymlink: false), true)

        // None exist.

        try fs.removeFileTree(source)
        XCTAssertEqual(fs.exists(source), false)
        XCTAssertEqual(fs.exists(source, followSymlink: true), false)
        XCTAssertEqual(fs.exists(source, followSymlink: false), false)
    }

    func testSetAttribute() throws {
      #if canImport(Darwin) || os(Linux) || os(Android)
        try testWithTemporaryDirectory { tmpdir in
            let fs = TSCBasic.localFileSystem

            let dir = tmpdir.appending(component: "dir")
            let foo = dir.appending(component: "foo")
            let bar = dir.appending(component: "bar")
            let sym = dir.appending(component: "sym")

            try fs.createDirectory(dir, recursive: true)
            try fs.writeFileContents(foo, bytes: "")
            try fs.writeFileContents(bar, bytes: "")
            try fs.createSymbolicLink(sym, pointingAt: foo, relative: false)

            // Set foo to unwritable.
            try fs.chmod(.userUnWritable, path: foo)
            XCTAssertThrows(FileSystemError(.invalidAccess, foo)) {
                try fs.writeFileContents(foo, bytes: "test")
            }

            // Set the directory as unwritable.
            try fs.chmod(.userUnWritable, path: dir, options: [.recursive, .onlyFiles])
            XCTAssertThrows(FileSystemError(.invalidAccess, bar)) {
                try fs.writeFileContents(bar, bytes: "test")
            }

            // Ensure we didn't modify foo's permission through the symlink.
            XCTAssertFalse(fs.isExecutableFile(foo))

            // It should be possible to add files.
            try fs.writeFileContents(dir.appending(component: "new"), bytes: "")

            // But not anymore.
            try fs.chmod(.userUnWritable, path: dir, options: [.recursive])
            let newFile = dir.appending(component: "new2")
            XCTAssertThrows(FileSystemError(.invalidAccess, newFile)) {
                try fs.writeFileContents(newFile, bytes: "")
            }

            try? fs.removeFileTree(bar)
            try? fs.removeFileTree(dir)
            XCTAssertTrue(fs.exists(dir))
            XCTAssertTrue(fs.exists(bar))

            // Set the entire directory as writable.
            try fs.chmod(.userWritable, path: dir, options: [.recursive])
            try fs.writeFileContents(foo, bytes: "test")
            try fs.removeFileTree(dir)
            XCTAssertFalse(fs.exists(dir))
        }
      #endif
    }

    func testInMemoryFileSystemFileLock() throws {
        let fs = InMemoryFileSystem()
        let path = AbsolutePath("/")
        try fs.createDirectory(path)

        let fileA = path.appending(component: "fileA")
        let fileB = path.appending(component: "fileB")
        let lockFile = path.appending(component: "lockfile")

        try _testFileSystemFileLock(fileSystem: fs, fileA: fileA, fileB: fileB, lockFile: lockFile)
    }

    func testLocalFileSystemFileLock() throws {
        try withTemporaryDirectory { tempDir in
            let fileA = tempDir.appending(component: "fileA")
            let fileB = tempDir.appending(component: "fileB")
            let lockFile = tempDir.appending(component: "lockfile")

            try _testFileSystemFileLock(fileSystem: localFileSystem, fileA: fileA, fileB: fileB, lockFile: lockFile)

            // Test some long and edge case paths. We arrange to split between the C and the Cedilla by repeating 255 times.
            let longEdgeCase1 = tempDir.appending(component: String(repeating: "Façade!  ", count: 255).decomposedStringWithCanonicalMapping)
            try localFileSystem.withLock(on: longEdgeCase1, type: .exclusive, {})
            let longEdgeCase2 = tempDir.appending(component: String(repeating: "🏁", count: 255).decomposedStringWithCanonicalMapping)
            try localFileSystem.withLock(on: longEdgeCase2, type: .exclusive, {})
        }
    }

    func testRerootedFileSystemViewFileLock() throws {
        let inMemoryFS = InMemoryFileSystem()
        let rootPath = AbsolutePath("/tmp")
        try inMemoryFS.createDirectory(rootPath)

        let fs = RerootedFileSystemView(inMemoryFS, rootedAt: rootPath)
        let path = AbsolutePath("/")
        try fs.createDirectory(path)

        let fileA = path.appending(component: "fileA")
        let fileB = path.appending(component: "fileB")
        let lockFile = path.appending(component: "lockfile")

        try _testFileSystemFileLock(fileSystem: fs, fileA: fileA, fileB: fileB, lockFile: lockFile)
    }

    private func _testFileSystemFileLock(fileSystem fs: FileSystem, fileA: AbsolutePath, fileB: AbsolutePath, lockFile: AbsolutePath) throws {
        // write initial value, since reader may start before writers and files would not exist
        try fs.writeFileContents(fileA, bytes: "0")
        try fs.writeFileContents(fileB, bytes: "0")

        let writerThreads = (0..<100).map { _ in
            return Thread {
                try! fs.withLock(on: lockFile, type: .exclusive) {
                    // Get the current contents of the file if any.
                    let valueA = Int(try fs.readFileContents(fileA).description)!

                    // Sum and write back to file.
                    try fs.writeFileContents(fileA, bytes: ByteString(encodingAsUTF8: String(valueA + 1)))

                    Thread.yield()

                    // Get the current contents of the file if any.
                    let valueB = Int(try fs.readFileContents(fileB).description)!

                    // Sum and write back to file.
                    try fs.writeFileContents(fileB, bytes: ByteString(encodingAsUTF8: String(valueB + 1)))
                }
            }
        }

        let readerThreads = (0..<20).map { _ in
            return Thread {
                try! fs.withLock(on: lockFile, type: .shared) {
                    try XCTAssertEqual(fs.readFileContents(fileA), fs.readFileContents(fileB))

                    Thread.yield()

                    try XCTAssertEqual(fs.readFileContents(fileA), fs.readFileContents(fileB))
                }
            }
        }

        writerThreads.forEach { $0.start() }
        readerThreads.forEach { $0.start() }
        writerThreads.forEach { $0.join() }
        readerThreads.forEach { $0.join() }

        try XCTAssertEqual(fs.readFileContents(fileA), "100")
        try XCTAssertEqual(fs.readFileContents(fileB), "100")
    }
}

/// Helper method to test file tree removal method on the given file system.
///
/// - Parameters:
///   - fs: The filesystem to test on.
///   - basePath: The path at which the temporary file strucutre should be created.
private func removeFileTreeTester(fs: FileSystem, basePath path: AbsolutePath, file: StaticString = #file, line: UInt = #line) throws {
    // Test removing folders.
    let folders = path.appending(components: "foo", "bar", "baz")
    try fs.createDirectory(folders, recursive: true)
    XCTAssert(fs.exists(folders), file: file, line: line)
    try fs.removeFileTree(folders)
    XCTAssertFalse(fs.exists(folders), file: file, line: line)

    // Test removing file.
    let filePath = folders.appending(component: "foo.txt")
    try fs.createDirectory(folders, recursive: true)
    try fs.writeFileContents(filePath, bytes: "foo")
    XCTAssert(fs.exists(filePath), file: file, line: line)
    try fs.removeFileTree(filePath)
    XCTAssertFalse(fs.exists(filePath), file: file, line: line)

    // Test removing non-existent path.
    let nonExistingPath = folders.appending(component: "does-not-exist")
    XCTAssertNoThrow(try fs.removeFileTree(nonExistingPath))
}
