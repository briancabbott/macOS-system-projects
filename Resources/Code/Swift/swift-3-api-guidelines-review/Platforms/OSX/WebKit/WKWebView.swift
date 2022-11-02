
@available(OSX 10.10, *)
class WKWebView : NSView {
  @NSCopying var configuration: WKWebViewConfiguration { get }
  weak var navigationDelegate: @sil_weak WKNavigationDelegate?
  weak var uiDelegate: @sil_weak WKUIDelegate?
  var backForwardList: WKBackForwardList { get }
  init(frame frame: CGRect, configuration configuration: WKWebViewConfiguration)
  func load(_ request: NSURLRequest) -> WKNavigation?
  @available(OSX 10.11, *)
  func loadFileURL(_ URL: NSURL, allowingReadAccessTo readAccessURL: NSURL) -> WKNavigation?
  func loadHTMLString(_ string: String, baseURL baseURL: NSURL?) -> WKNavigation?
  @available(OSX 10.11, *)
  func load(_ data: NSData, mimeType MIMEType: String, characterEncodingName characterEncodingName: String, baseURL baseURL: NSURL) -> WKNavigation?
  func go(to item: WKBackForwardListItem) -> WKNavigation?
  var title: String? { get }
  @NSCopying var url: NSURL? { get }
  var isLoading: Bool { get }
  var estimatedProgress: Double { get }
  var hasOnlySecureContent: Bool { get }
  @available(OSX 10.11, *)
  var certificateChain: [AnyObject] { get }
  var canGoBack: Bool { get }
  var canGoForward: Bool { get }
  func goBack() -> WKNavigation?
  func goForward() -> WKNavigation?
  func reload() -> WKNavigation?
  func reloadFromOrigin() -> WKNavigation?
  func stopLoading()
  func evaluateJavaScript(_ javaScriptString: String, completionHandler completionHandler: ((AnyObject?, NSError?) -> Void)? = nil)
  var allowsBackForwardNavigationGestures: Bool
  @available(OSX 10.11, *)
  var customUserAgent: String?
  @available(OSX 10.11, *)
  var allowsLinkPreview: Bool
  var allowsMagnification: Bool
  var magnification: CGFloat
  func setMagnification(_ magnification: CGFloat, centeredAt point: CGPoint)
}
extension WKWebView : NSUserInterfaceValidations {
  @IBAction func goBack(_ sender: AnyObject?)
  @IBAction func goForward(_ sender: AnyObject?)
  @IBAction func reload(_ sender: AnyObject?)
  @IBAction func reloadFromOrigin(_ sender: AnyObject?)
  @IBAction func stopLoading(_ sender: AnyObject?)
  @available(OSX 10.10, *)
  func validate(_ anItem: NSValidatedUserInterfaceItem) -> Bool
}
