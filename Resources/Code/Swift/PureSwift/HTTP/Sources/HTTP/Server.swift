//
//  HTTPServer.swift
//  
//
//  Created by Alsey Coleman Miller on 10/3/22.
//

import Foundation
import Socket

public final class HTTPServer {
    
    // MARK: - Properties
    
    public let configuration: Configuration
    
    internal let response: (IPAddress, HTTPRequest) async -> (HTTPResponse)
    
    internal let log: (String) -> ()
    
    internal let socket: Socket
    
    private var task: Task<(), Never>?
    
    let storage = Storage()
    
    // MARK: - Initialization
    
    deinit {
        stop()
    }
    
    public init(
        configuration: Configuration = .init(),
        log: ((String) -> ())? = nil,
        response: ((IPAddress, HTTPRequest) async -> HTTPResponse)? = nil
    ) async throws {
        let log = log ?? {
            #if DEBUG
            print("HTTPServer: \($0)")
            #endif
        }
        self.configuration = configuration
        self.response = response ?? { (address, request) in
            #if DEBUG
            log("[\(address)] \(request.method) \(request.uri)")
            #endif
            return .init(code: .ok)
        }
        self.log = log
        // create listening socket
        self.socket = try await Socket(.tcp, bind: IPv4SocketAddress(address: .any, port: configuration.port))
        try socket.fileDescriptor.listen(backlog: configuration.backlog)
        // start running server
        start()
    }
    
    // MARK: - Methods
    
    private func start() {
        assert(task == nil)
        // listening run loop
        self.task = Task.detached(priority: .high) { [weak self] in
            self?.log("Started HTTP Server")
            do {
                while let socket = self?.socket {
                    try Task.checkCancellation()
                    // wait for incoming sockets
                    let newSocket = try await Socket(fileDescriptor: socket.fileDescriptor.accept())
                    self?.log("New connection")
                    if let self = self {
                        try await self.storage.newConnection(newSocket, server: self)
                    }
                }
            }
            catch _ as CancellationError { }
            catch {
                self?.log("Error waiting for new connection: \(error)")
            }
        }
    }
    
    public func stop() {
        assert(task != nil)
        let socket = self.socket
        self.task?.cancel()
        self.task = nil
        self.log("Stopped GATT Server")
        Task { await socket.close() }
    }
}

internal extension HTTPServer {
    
    func connection(_ address: IPAddress, didDisconnect error: Swift.Error?) async {
        // remove connection cache
        await storage.removeConnection(address)
        // log
        log("[\(address)]: " + "Did disconnect. \(error?.localizedDescription ?? "")")
    }
}

// MARK: - Supporting Types

public extension HTTPServer {
    
    struct Configuration: Equatable, Hashable, Codable {
        
        public let port: UInt16
                
        public let backlog: Int
        
        public let headerMaxSize: Int
        
        public let bodyMaxSize: Int
        
        public init(
            port: UInt16 = 8080,
            backlog: Int = 10_000,
            headerMaxSize: Int = 4096,
            bodyMaxSize: Int = 1024 * 1024 * 2
        ) {
            self.port = port
            self.backlog = backlog
            self.headerMaxSize = headerMaxSize
            self.bodyMaxSize = bodyMaxSize
        }
    }
}

internal extension HTTPServer {
    
    actor Storage {
        
        var connections = [IPAddress: Connection](minimumCapacity: 100)
        
        fileprivate init() { }
        
        func newConnection(_ socket: Socket, server: HTTPServer) async throws {
            // read remote address
            let address = try socket.fileDescriptor.peerAddress(IPv4SocketAddress.self).address // TODO: Support IPv6
            // create connection actor
            connections[.v4(address)] = await Connection(address: .v4(address), socket: socket, server: server)
        }
        
        func removeConnection(_ address: IPAddress) {
            self.connections[address] = nil
        }
    }
}

internal extension HTTPServer {
    
    actor Connection {
        
        // MARK: - Properties
        
        let address: IPAddress
        
        let socket: Socket
        
        private weak var server: HTTPServer?
        
        let configuration: Configuration
        
        private(set) var isConnected = true
        
        private(set) var readData = Data()
        
        // MARK: - Initialization
        
        deinit {
            let socket = self.socket
            Task { await socket.close() }
        }
        
        init(
            address: IPAddress,
            socket: Socket,
            server: HTTPServer
        ) async {
            self.address = address
            self.socket = socket
            self.server = server
            self.configuration = server.configuration
            await run()
        }
        
        private func run() {
            // start reading
            Task {
                await run()
            }
            Task.detached(priority: .high) { [weak self] in
                guard let stream = self?.socket.event else { return }
                for await event in stream {
                    await self?.socketEvent(event)
                }
                // socket closed
            }
        }
        
        private func socketEvent(_ event: Socket.Event) async {
            switch event {
            case .pendingRead:
                break
            case .read:
                break
            case .write:
                break
            case let .close(error):
                isConnected = false
                await server?.connection(address, didDisconnect: error)
            }
        }
        
        private func read(_ length: Int) async throws {
            let data = try await socket.read(length)
            self.server?.log("[\(address)] Read \(data.count) bytes")
            self.readData.append(data)
        }
        
        private func respond(_ response: HTTPResponse) async throws {
            let data = response.data
            self.server?.log("[\(address)] Response \(response.code.rawValue) \(response.status) (\(data.count) bytes)")
            try await socket.write(data)
            await socket.close()
        }
        
        private func respond(_ code: HTTPStatusCode) async throws {
            try await respond(HTTPResponse(code: code))
        }
        
        private func run() async {
            let headerMaxSize = configuration.headerMaxSize
            let initialReadSize = min(headerMaxSize, 512)
            do {
                // read small chunk
                try await read(initialReadSize)
                // read remaning
                if initialReadSize < headerMaxSize,
                   readData.contains(HTTPMessage.Decoder.cr) == false,
                   readData.last != HTTPMessage.Decoder.nl,
                   readData.count < headerMaxSize  {
                    let remainingSize = readData.count - headerMaxSize
                    try await read(remainingSize)
                }
                // verify header
                guard let headerEndIndex = readData.firstIndex(of: HTTPMessage.Decoder.cr).map({ $0 + 1 }),
                      readData.count > headerEndIndex else { // for newline
                    try await respond(.payloadTooLarge)
                    return
                }
                guard var request = HTTPRequest(data: readData) else {
                    try await respond(.badRequest)
                    return
                }
                // get body
                if let contentLength = request.headers[.contentLength].flatMap(Int.init), contentLength > 0 {
                    guard contentLength > configuration.bodyMaxSize else {
                        try await respond(.payloadTooLarge)
                        return
                    }
                    let targetSize = headerEndIndex + contentLength + 1
                    let remainder = targetSize - readData.count
                    if remainder > 0 {
                        try await read(remainder)
                    }
                    request.body = readData.suffix(contentLength)
                } else {
                    request.body = Data()
                }
                // respond
                guard let responseHandler = self.server?.response else {
                    assertionFailure()
                    try await respond(.internalServerError)
                    return
                }
                let response = await responseHandler(address, request)
                try await respond(response)
            } catch {
                self.server?.log("[\(address)] \(error.localizedDescription)")
                await self.socket.close()
            }
        }
    }
}
