import XCTest
@testable import JSONSchema

final class JSONSchemaTests: XCTestCase {
    
    static var allTests = [
        ("testReference", testReference),
        ("testSchemaParse", testSchemaParse)
        ]
    
    func testReference() {
        
        // invalid strings
        do {
            
            XCTAssertNil(Reference(rawValue: ""))
            XCTAssertNil(Reference(rawValue: "/"))
            XCTAssertNil(Reference(rawValue: "https://google.com"))
            XCTAssertNil(Reference(rawValue: "http://json-schema.org/draft-04/schema"))
            XCTAssertNil(Reference(rawValue: "http://json-schema.org/draft-04/schema"))
            XCTAssertNil(Reference(rawValue: "/definitions/schemaArray"))
        }
        
        do {
            
            let rawValue = "#"
            
            guard let reference = Reference(rawValue: rawValue)
                else { XCTFail("Could not parse"); return }
            
            XCTAssert(reference.rawValue == rawValue, "\(reference.rawValue)")
            XCTAssert(reference.path.isEmpty)
            XCTAssert(reference.remote == nil)
            XCTAssert(reference == .selfReference)
            XCTAssert(reference.rawValue == Reference.selfReference.rawValue)
        }
        
        do {
            let rawValue = "http://json-schema.org/draft-04/schema#/properties/title"
            
            guard let reference = Reference(rawValue: rawValue)
                else { XCTFail("Could not parse"); return }
            
            XCTAssert(reference.rawValue == rawValue, "\(reference.rawValue)")
            XCTAssert(reference.path == ["properties", "title"])
            XCTAssert(reference.remote?.absoluteString == "http://json-schema.org/draft-04/schema")
            XCTAssert(reference != .selfReference)
            XCTAssert(reference == "http://json-schema.org/draft-04/schema#/properties/title")
        }
        
        do {
            
            let rawValue = "#/definitions/schemaArray"
            
            guard let reference = Reference(rawValue: rawValue)
                else { XCTFail("Could not parse"); return }
            
            XCTAssert(reference.rawValue == rawValue, "\(reference.rawValue)")
            XCTAssert(reference.path == ["definitions", "schemaArray"])
            XCTAssert(reference.remote == nil)
            XCTAssert(reference != .selfReference, "\(reference)")
            XCTAssert(reference == "#/definitions/schemaArray")
        }
    }
    
    func testSchemaParse() {
        
        typealias Schema = JSONSchema.Draft4.Object
        
        let jsonSchemaURLs = [
            "http://json-schema.org/draft-04/schema#",
            "http://swagger.io/v2/schema.json#"
        ]
        
        for urlString in jsonSchemaURLs {
            
            let url = URL(string: urlString)!
            
            guard let jsonData = try? Data(contentsOf: url)
                else { XCTFail("Could not fetch \(url)"); continue }
            
            let jsonDecoder = JSONDecoder()
            
            var scheme: Schema!
            
            // parse
            do { scheme = try jsonDecoder.decode(Schema.self, from: jsonData) }
                
            catch {
                dump(error)
                XCTFail()
                return
            }
            
            dump(scheme)
            
            // resolve references
            do {
                
                let references = try scheme.resolveReferences()
                
                print("\(references.count) references in \(scheme.identifier?.absoluteString ?? "Unknown Schema")")
                dump(Array(references.keys))
                
            } catch {
                dump(error)
                XCTFail()
                return
            }
        }
    }
}
