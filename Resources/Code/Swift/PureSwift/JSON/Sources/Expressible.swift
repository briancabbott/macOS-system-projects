

extension JSON.Value: ExpressibleByArrayLiteral {
  public init(arrayLiteral elements: JSONEncodable...) {
    let array = elements.map({ $0.toJSON() })
    self = .array(array)
  }
}

extension JSON.Value: ExpressibleByDictionaryLiteral {
  public init(dictionaryLiteral elements: (String, JSONEncodable)...) {

    var object: JSON.Object = JSON.Object(minimumCapacity: elements.count)
    
    elements.forEach { object[$0.0] = $0.1.toJSON() }
    
    self = .object(object)
  }
}

extension JSON.Value: ExpressibleByIntegerLiteral {
  public init(integerLiteral value: IntegerLiteralType) {
    let val = Int64(value)
    self = .integer(val)
  }
}

extension JSON.Value: ExpressibleByFloatLiteral {
  public init(floatLiteral value: FloatLiteralType) {
    let val = Double(value)
    self = .double(val)
  }
}

extension JSON.Value: ExpressibleByStringLiteral {
  public init(stringLiteral value: String) {
    self = .string(value)
  }

  public init(extendedGraphemeClusterLiteral value: String) {
    self = .string(value)
  }

  public init(unicodeScalarLiteral value: String) {
    self = .string(value)
  }
}

extension JSON.Value: ExpressibleByBooleanLiteral {
  public init(booleanLiteral value: Bool) {
    self = .boolean(value)
  }
}


// MARK: - JSON: CustomStringConvertible

extension JSON.Value: CustomStringConvertible {
  public var description: String {
    do {
      return try self.toString()
    } catch {
      return String(describing: error)
    }
  }
}

extension JSON.Value: CustomDebugStringConvertible {
  public var debugDescription: String {
    do {
      return try self.toString(options: .prettyPrint)
    } catch {
      return String(describing: error)
    }
  }
}
