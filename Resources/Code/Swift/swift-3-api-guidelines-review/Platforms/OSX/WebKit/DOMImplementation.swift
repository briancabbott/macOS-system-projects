
@available(OSX 10.4, *)
class DOMImplementation : DOMObject {
  @available(OSX 10.5, *)
  func hasFeature(_ feature: String!, version version: String!) -> Bool
  @available(OSX 10.5, *)
  func createDocumentType(_ qualifiedName: String!, publicId publicId: String!, systemId systemId: String!) -> DOMDocumentType!
  @available(OSX 10.5, *)
  func createDocument(_ namespaceURI: String!, qualifiedName qualifiedName: String!, doctype doctype: DOMDocumentType!) -> DOMDocument!
  @available(OSX 10.5, *)
  func createCSSStyleSheet(_ title: String!, media media: String!) -> DOMCSSStyleSheet!
  @available(OSX 10.5, *)
  func createHTMLDocument(_ title: String!) -> DOMHTMLDocument!
}
extension DOMImplementation {
}
