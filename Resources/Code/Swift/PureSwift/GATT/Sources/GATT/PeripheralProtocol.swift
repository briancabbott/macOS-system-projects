//
//  Peripheral.swift
//  GATT
//
//  Created by Alsey Coleman Miller on 4/1/16.
//  Copyright © 2016 PureSwift. All rights reserved.
//

#if canImport(BluetoothGATT)
import Foundation
@_exported import Bluetooth
@_exported import BluetoothGATT

/// GATT Peripheral Manager
///
/// Implementation varies by operating system.
public protocol PeripheralManager: AnyObject {
    
    /// Central Peer
    ///
    /// Represents a remote central device that has connected to an app implementing the peripheral role on a local device.
    associatedtype Central: Peer
    
    /// Start advertising the peripheral and listening for incoming connections.
    func start() async throws
    
    /// Stop the peripheral.
    func stop()
    
    /// Attempts to add the specified service to the GATT database.
    ///
    /// - Returns: Attribute handle.
    func add(service: BluetoothGATT.GATTAttribute.Service) async throws -> UInt16
    
    /// Removes the service with the specified handle.
    func remove(service: UInt16) async
    
    /// Clears the local GATT database.
    func removeAllServices() async
    
    var willRead: ((GATTReadRequest<Central>) async -> ATTError?)? { get set }
    
    var willWrite: ((GATTWriteRequest<Central>) async -> ATTError?)? { get set }
    
    var didWrite: ((GATTWriteConfirmation<Central>) async -> ())? { get set }
    
    /// Modify the value of a characteristic, optionally emiting notifications if configured on active connections.
    func write(_ newValue: Data, forCharacteristic handle: UInt16) async
    
    /// Read the value of the characteristic with specified handle.
    subscript(characteristic handle: UInt16) -> Data { get async }
    
    /// Return the handles of the characteristics matching the specified UUID.
    func characteristics(for uuid: BluetoothUUID) async -> [UInt16]
}

// MARK: - Supporting Types

public protocol GATTRequest {
    
    associatedtype Central: Peer
    
    var central: Central { get }
    
    var maximumUpdateValueLength: Int { get }
    
    var uuid: BluetoothUUID { get }
    
    var handle: UInt16 { get }
    
    var value: Data { get }
}

public struct GATTReadRequest <Central: Peer> : GATTRequest {
    
    public let central: Central
    
    public let maximumUpdateValueLength: Int
    
    public let uuid: BluetoothUUID
    
    public let handle: UInt16
    
    public let value: Data
    
    public let offset: Int
    
    public init(central: Central,
                maximumUpdateValueLength: Int,
                uuid: BluetoothUUID,
                handle: UInt16,
                value: Data,
                offset: Int) {
        
        self.central = central
        self.maximumUpdateValueLength = maximumUpdateValueLength
        self.uuid = uuid
        self.handle = handle
        self.value = value
        self.offset = offset
    }
}

public struct GATTWriteRequest <Central: Peer> : GATTRequest {
    
    public let central: Central
    
    public let maximumUpdateValueLength: Int
    
    public let uuid: BluetoothUUID
    
    public let handle: UInt16
    
    public let value: Data
    
    public let newValue: Data
    
    public init(central: Central,
                maximumUpdateValueLength: Int,
                uuid: BluetoothUUID,
                handle: UInt16,
                value: Data,
                newValue: Data) {
        
        self.central = central
        self.maximumUpdateValueLength = maximumUpdateValueLength
        self.uuid = uuid
        self.handle = handle
        self.value = value
        self.newValue = newValue
    }
}

public struct GATTWriteConfirmation <Central: Peer> : GATTRequest {
    
    public let central: Central
    
    public let maximumUpdateValueLength: Int
    
    public let uuid: BluetoothUUID
    
    public let handle: UInt16
    
    public let value: Data
    
    public init(central: Central,
                maximumUpdateValueLength: Int,
                uuid: BluetoothUUID,
                handle: UInt16,
                value: Data) {
        
        self.central = central
        self.maximumUpdateValueLength = maximumUpdateValueLength
        self.uuid = uuid
        self.handle = handle
        self.value = value
    }
}

#endif
