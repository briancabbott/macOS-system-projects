//
//  VisibilityModifiers.swift
//  
//
//  Created by Carson Katri on 5/28/20.
//

public extension View {
  func onAppear(perform action: (() -> Void)? = nil) -> some View {
    return modifier(OnLoadModifier(action: action ?? {}))
  }
}

struct OnLoadModifier: ViewModifier {
  let action: () -> Void
  
  func buildTree<T: View>(for view: T, in context: TreeStateContext) -> HTMLTreeNode {
    // This is hacked in pretty poorly, but it works.
    // We really need a way to track this per-View.
    // Not sure how to go about that yet.
    if context.buildOrigin == .initial {
      action()
    }
    return context.currentBuilder.buildTree(for: view, in: context)
  }
}

