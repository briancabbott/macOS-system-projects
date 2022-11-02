
@available(watchOS 2.0, *)
struct NSFileWrapperReadingOptions : OptionSetType {
  init(rawValue rawValue: UInt)
  let rawValue: UInt
  static var immediate: NSFileWrapperReadingOptions { get }
  static var withoutMapping: NSFileWrapperReadingOptions { get }
}
@available(watchOS 2.0, *)
struct NSFileWrapperWritingOptions : OptionSetType {
  init(rawValue rawValue: UInt)
  let rawValue: UInt
  static var atomic: NSFileWrapperWritingOptions { get }
  static var withNameUpdating: NSFileWrapperWritingOptions { get }
}
@available(watchOS 2.0, *)
class NSFileWrapper : NSObject, NSCoding {
  @available(watchOS 2.0, *)
  init(url url: NSURL, options options: NSFileWrapperReadingOptions = []) throws
  init(directoryWithFileWrappers childrenByPreferredName: [String : NSFileWrapper])
  init(regularFileWithContents contents: NSData)
  @available(watchOS 2.0, *)
  init(symbolicLinkWithDestinationURL url: NSURL)
  init?(serializedRepresentation serializeRepresentation: NSData)
  init?(coder inCoder: NSCoder)
  var isDirectory: Bool { get }
  var isRegularFile: Bool { get }
  var isSymbolicLink: Bool { get }
  var preferredFilename: String?
  var filename: String?
  var fileAttributes: [String : AnyObject]
  @available(watchOS 2.0, *)
  func matchesContents(of url: NSURL) -> Bool
  @available(watchOS 2.0, *)
  func read(from url: NSURL, options options: NSFileWrapperReadingOptions = []) throws
  @available(watchOS 2.0, *)
  func write(to url: NSURL, options options: NSFileWrapperWritingOptions = [], originalContentsURL originalContentsURL: NSURL?) throws
  @NSCopying var serializedRepresentation: NSData? { get }
  func addFileWrapper(_ child: NSFileWrapper) -> String
  func addRegularFile(withContents data: NSData, preferredFilename fileName: String) -> String
  func removeFileWrapper(_ child: NSFileWrapper)
  var fileWrappers: [String : NSFileWrapper]? { get }
  func key(for child: NSFileWrapper) -> String?
  @NSCopying var regularFileContents: NSData? { get }
  @available(watchOS 2.0, *)
  @NSCopying var symbolicLinkDestinationURL: NSURL? { get }
  @available(watchOS 2.0, *)
  func encode(with aCoder: NSCoder)
}
