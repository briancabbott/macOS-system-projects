//
//  TabItemLabel.swift
//  SwiftWebUI
//
//  Created by Helge Heß on 18.06.19.
//  Copyright © 2019-2020 Helge Heß. All rights reserved.
//

public extension View {

  func tabItem<V: View>(_ item: V) -> some View {
    // The official doesn't carry a type here, but just `AnyView?`. Hm.
    return modifier(TraitWritingModifier(content:
                      TabItemLabel(value: AnyView(item))))
  }
  
  @available(*, deprecated, renamed: "tabItem")
  func tabItemLabel<V: View>(_ item: V) -> some View {
    return modifier(TraitWritingModifier(content:
                      TabItemLabel(value: AnyView(item))))
  }
}

public struct TabItemLabel: Trait {
  public typealias Value = AnyView
  let value : AnyView
}
