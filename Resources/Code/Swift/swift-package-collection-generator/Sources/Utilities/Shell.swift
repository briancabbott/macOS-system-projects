//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift Package Collection Generator open source project
//
// Copyright (c) 2020 Apple Inc. and the Swift Package Collection Generator project authors
// Licensed under Apache License v2.0
//
// See LICENSE.txt for license information
// See CONTRIBUTORS.txt for the list of Swift Package Collection Generator project authors
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

import Foundation
import TSCBasic

public enum ShellUtilities {
    public static let shell = ProcessInfo.processInfo.environment["PACKAGE_COLLECTION_GENERATOR_SHELL"] ?? "bash"

    @discardableResult
    public static func run(_ arguments: String...) throws -> String {
        try Process.checkNonZeroExit(arguments: arguments)
    }
}
