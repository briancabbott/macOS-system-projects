
protocol UIActivityItemSource : NSObjectProtocol {
  @available(iOS 6.0, *)
  func activityViewControllerPlaceholderItem(_ activityViewController: UIActivityViewController) -> AnyObject
  @available(iOS 6.0, *)
  func activityViewController(_ activityViewController: UIActivityViewController, itemForActivityType activityType: String) -> AnyObject?
  @available(iOS 6.0, *)
  optional func activityViewController(_ activityViewController: UIActivityViewController, subjectForActivityType activityType: String?) -> String
  @available(iOS 6.0, *)
  optional func activityViewController(_ activityViewController: UIActivityViewController, dataTypeIdentifierForActivityType activityType: String?) -> String
  @available(iOS 6.0, *)
  optional func activityViewController(_ activityViewController: UIActivityViewController, thumbnailImageForActivityType activityType: String?, suggestedSize size: CGSize) -> UIImage?
}
@available(iOS 6.0, *)
class UIActivityItemProvider : NSOperation, UIActivityItemSource {
  init(placeholderItem placeholderItem: AnyObject)
  var placeholderItem: AnyObject? { get }
  var activityType: String? { get }
  func item() -> AnyObject
  @available(iOS 6.0, *)
  func activityViewControllerPlaceholderItem(_ activityViewController: UIActivityViewController) -> AnyObject
  @available(iOS 6.0, *)
  func activityViewController(_ activityViewController: UIActivityViewController, itemForActivityType activityType: String) -> AnyObject?
  @available(iOS 6.0, *)
  func activityViewController(_ activityViewController: UIActivityViewController, subjectForActivityType activityType: String?) -> String
  @available(iOS 6.0, *)
  func activityViewController(_ activityViewController: UIActivityViewController, dataTypeIdentifierForActivityType activityType: String?) -> String
  @available(iOS 6.0, *)
  func activityViewController(_ activityViewController: UIActivityViewController, thumbnailImageForActivityType activityType: String?, suggestedSize size: CGSize) -> UIImage?
}
