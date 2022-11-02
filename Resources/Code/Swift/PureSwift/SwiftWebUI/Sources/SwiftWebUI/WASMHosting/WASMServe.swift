//
//  File.swift
//  
//
//  Created by Carson Katri on 5/27/20.
//

import JavaScriptKit

/// The node that we should inject the generated HTML into.
/// This is to allow SwiftWebUI to be embedded into existing webapps
public struct HostNode {
    let id: String
    
    public init(id: String) {
        self.id = id
    }
}

// Overload print to use the JavaScript console
let console = JSObjectRef.global.console.object!

public func print(_ items: String...) {
    _ = console.log!(items.joined(separator: "\n"))
}
func print(_ item: JSValue) {
    _ = console.log!(item)
}
func print(_ item: JSObjectRef) {
    _ = console.log!(item)
}

final class WASMHost {
  let treeContext : TreeStateContext
  var tree        : HTMLTreeNode
  let rootView    : AnyView
  
  let document = JSObjectRef.global.document.object!

  init<RootView: View>(view: RootView) {
    self.rootView  = AnyView(view)
    
    let treeContext = TreeStateContext()

    self.treeContext = treeContext
    
    if debugRequestPhases { print("RP: Build initial tree ...") }
    self.tree = treeContext.currentBuilder.buildTree(for: view, in: treeContext)
    if debugRequestPhases {
      print("RP: Built initial tree:")
      self.tree.dump()
    }
    
    #if DEBUG && false
      self.tree.dump()
    #endif
  }
  
  // MARK: - Initial HTML Page Generation
  
  struct SwiftUIEvent {
      enum EventType: String {
          case click
          case commit
          case change
      }
      let event : EventType
      let webID : [ String ]
      let value : String?
      
      init(event: EventType, webID: String, value: String? = nil) {
          self.event = event
          self.webID = webID.split(separator: ".").map(String.init)
          self.value = value
      }
  }
  
  func sendInitialPage(to hostNode: HostNode) throws {
    var html = ""
    html.reserveCapacity(32000)
    try append(to: &html)
    if let node = document.getElementById!(hostNode.id).object {
      node.innerHTML = .string(html)
    }
    JSObjectRef.global.SwiftUIClick = JSValue.function { props in
      if props.count > 1, let element = props[0].object as? JSObjectRef, let event = props[1].object as? JSObjectRef {
        event.stopPropagation!()
        guard let id = element.id.string else {
          return .undefined
        }
        do {
          try self.receiveEvent(.click, elementID: id)
        } catch {
         print(error)
       }
      }
      return .undefined
    }
    let receiveEventTarget = JSValue.function { props in
      if props.count > 1, let event = props[1].object as? JSObjectRef {
        if let target = event.target.object, let value = target.value.string {
          do {
            try self.receiveEvent(.click, elementID: value)
          } catch {
            print(error)
          }
        }
      }
      return .undefined
    }
    JSObjectRef.global.SwiftUIRadioChanged = receiveEventTarget
    JSObjectRef.global.SwiftUISelectChanged = receiveEventTarget
    JSObjectRef.global.SwiftUICheckboxChanged = JSValue.function { props in
      if props.count > 0, let element = props[0].object as? JSObjectRef {
        if let id = element.id.string, let checked = element.checked.boolean {
          do {
            try self.receiveEvent(.click, elementID: id, value: checked ? "on" : "off")
          } catch {
            print(error)
          }
        }
      }
      return .undefined
    }
    JSObjectRef.global.SwiftUITabClick = JSValue.function { props in
      if props.count > 1, let tabItem = props[0].object as? JSObjectRef, let event = props[1].object as? JSObjectRef {
        event.stopPropagation!()
        if let element = self.document.getElementById!(tabItem.getAttribute!("data-tab")).object {
          func dropClassFromChildren(of node: JSObjectRef, className: String) {
            var _child = node.firstChild.object
            while let child = _child {
              if let nodeType = child.nodeType.number, Int(nodeType) == 1 {
                child.classList.object!.remove!(className)
              }
              _child = child.nextSibling.object
            }
          }

          let tabItemId = tabItem.id.string
          let elementId = element.id.string
          if let parent = element.parentNode.object {
            dropClassFromChildren(of: parent, className: "active")
          }
          if let parent = tabItem.parentNode.object {
            dropClassFromChildren(of: parent, className: "active")
          }
          if let tabItemId = tabItemId {
            self.document.getElementById!(tabItemId).object?.classList.object?.add?("active")
          }
          if let elementId = elementId {
            self.document.getElementById!(elementId).object?.classList.object?.add?("active")
          }
        }
      }
      return .undefined
    }
    JSObjectRef.global.SwiftUIPossibleStateChange = JSValue.function { props in
      do {
        try self.receiveEvent(.change, elementID: "/")
      } catch {
        print(error)
      }
      return .undefined
    }
    let inputChanged = JSValue.function { props in
      if props.count > 0, let element = props[0].object as? JSObjectRef {
        if let id = element.id.string, let value = element.value.string {
          do {
            try self.receiveEvent(.commit, elementID: id, value: value)
          } catch {
            print(error)
          }
        }
      }
      return .undefined
    }
    JSObjectRef.global.SwiftUIValueCommit = inputChanged
    JSObjectRef.global.SwiftUIValueChanged = inputChanged
  }
  
  // MARK: - Handle DOM Events
  func receiveEvent(_ type: SwiftUIEvent.EventType, elementID: String, value: String? = nil) throws {
      let event = SwiftUIEvent(event: type, webID: elementID, value: value)
      // takeValuesFromRequest
      if let value = event.value {
      if debugRequestPhases { print("RP: taking value: \(value) ..") }
          try tree.takeValue(event.webID, value: value, in: treeContext)
          if debugRequestPhases { print("RP: did take value.") }
      }
      else if debugRequestPhases { print("RP: not taking values ..") }
      
    // invokeAction

    if debugRequestPhases {
      print("RP: invoke: \(event.webID.joined(separator: ".")) ..")
    }
    try tree.invoke(event.webID, in: treeContext)
    if debugRequestPhases { print("RP: did invoke.") }

    // check whether the invocation invalidated any components

    if treeContext.invalidComponentIDs.isEmpty {
      // Nothing changed!
      let gottoLoveCodable = [ String : String ]()
      if debugRequestPhases { print("RP: no invalid components, return") }
//        return response.json(gottoLoveCodable)
    }
    else if debugRequestPhases {
      print("RP: #\(treeContext.invalidComponentIDs.count) invalid components,",
            "generate changes:")
    }

    if debugDumpTrees {
      print("OLD TREE:")
      tree.dump()
      print("-----")
    }

    // generate changes in tree

    var changes = [ HTMLChange ]()

    #if false
      // OK, so this is unfortunate, we have the infrastructure to rerender
      // just the invalid component subtrees.
      // But that doesn't play well w/ tree rewriting because the rewriting
      // can overlap with components (which is kinda required functionality).
      // So for now, we just go the worst route and recreate a full new tree :-/

      tree.generateChanges(from: tree, into: &changes, in: treeContext)
    #else
      // So for now, we just go the worst route and recreate a full new tree :-/
      let oldTree = tree
      treeContext.clearAllInvalidComponentsPriorTreeRebuild()
      self.tree = treeContext.currentBuilder
                             .buildTree(for: rootView, in: treeContext)
      tree.generateChanges(from: oldTree, into: &changes, in: treeContext)
    #endif
    if debugRequestPhases {
      print("RP: did generate changes: #\(changes.count)")
    }

    if debugDumpTrees {
      print("NEW TREE:")
      tree.dump()
      print("-----")
      print("CHANGES:")
      for change in changes {
        print("  ", change)
      }
      print("-----")
    }

    // cleanup and deliver

    treeContext.clearDiffingStates()
      
    for change in changes {
      guard let handleJSON = JSObjectRef.global.SwiftUIHandleJSON.function else { return }
      handleJSON(change.jsonObject.jsValue())
    }
    if debugRequestPhases { print("RP: sent.") }
  }
  
  func appendStylesheet(_ name: String, to html: inout String) {
    html += "<link rel='stylesheet' type='text/css' href='/www/\(name)'>"
  }
  func appendScript(_ name: String, to html: inout String) {
    html += "<script language='JavaScript' type='text/javascript'"
    html += " src='/www/\(name)'></script>"
  }
  
  func append(to html: inout String) throws {
    // We don't do any doctype, body tags, or anything like that since we want the views to be embedded in the HostNode.
    do {
      // We do add styles though
      if let head = document.head.object {
          if let style = document.createElement!("style").object {
              style.innerHTML = .string(SwiftWebUIStyles)
              head.appendChild!(style)
          }
      }
      
      do {
        html +=
          """
          <noscript><h2>No JavaScript? That is 1337!</h2></noscript>
          """

        html += "<div id='swiftui-page' class='swiftui-page'>"; defer { html += "</div>" }

        tree.generateHTML(into: &html)
        
        // This is a small hack to allow for `onAppear`.
        self.treeContext.buildOrigin = .event
      }
    }
  }
}

extension SwiftWebUI {
  public static func serve<V: View>(_ view: V, hostNode: HostNode = HostNode(id: "page-root")) {
    let host = WASMHost(view: view)
    do {
      try host.sendInitialPage(to: hostNode)
    } catch {
      print(error)
    }
  }
}
