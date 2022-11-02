//
//  File.swift
//  
//
//  Created by Helge Heß on 27.06.19.
//

#if canImport(SemanticUI)
import SemanticUI
#endif

public enum SwiftWebUI {
  
  /**
   * The primary entry point. This gets a closure which creates the initial
   * view for a session.
   *
   * By using a closure you can setup session local storage and pass it into
   * the view hierarchy using an `.environmentObject`, for example:
   *
   *     SwiftWebUI.serve {
   *
   *       let myModel = MyModelObject()
   *
   *       struct MyPageView: some View {
   *         var body: some View {
   *           CoolView()
   *             .environmentObject(myModel)
   *         }
   *       }
   *     }
   */
#if canImport(NIO)
  public static func serve<V: View>(port : Int    = 1337,
                                    host : String = "0.0.0.0",
                                    waitUntilDone: Bool = true,
                                    sessionViewBuilder: @escaping () -> V)
  {
    let endpoint = NIOEndpoint.shared
    
    let fonts  = "themes/default/assets/fonts"
    let images = "themes/default/assets/images"
#if canImport(SemanticUI)
    let resources = [
      "semantic.min.css"             : SemanticUI.data_semantic_min_css,
      "components/icon.min.css"      : SemanticUI.data_icon_min_css,
      "\(fonts)/icons.woff2"         : SemanticUI.data_icons_woff2,
      "\(fonts)/icons.svg"           : SemanticUI.data_icons_svg,
      "\(fonts)/outline-icons.woff2" : SemanticUI.data_outline_icons_woff2,
      "\(fonts)/outline-icons.svg"   : SemanticUI.data_outline_icons_svg,
      "\(images)/flags.png"          : SemanticUI.data_flags_png
    ]
#else
    let resources = [String:String]()
#endif
    for ( name, value ) in resources {
      endpoint.expose(value, as: name)
    }
    
    endpoint.use(sessionViewBuilder)
    endpoint.listen(port, host)
    if waitUntilDone { endpoint.wait() }
  }

  /**
   * A primary entry point. Pass in the view you want SwiftWebUI to new
   * new sessions. Sample:
   *
   *     SwiftWebUI.serve(Text("Hello World"))
   * 
   * There is also a closure based variant which allows you to implement
   * per-session state.
   */
  public static func serve<V: View>(port : Int = 1337,
                                    host : String = "0.0.0.0",
                                    waitUntilDone: Bool = true,
                                    _ view: V)
  {
    serve(port: port, host: host, waitUntilDone: waitUntilDone) { return view }
  }
#endif
}
