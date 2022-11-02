// Copyright 2020 Tokamak contributors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

public protocol ViewModifier {
  typealias Content = _ViewModifier_Content<Self>
  associatedtype Body: View
  func body(content: Content) -> Self.Body
}

public struct _ViewModifier_Content<Modifier>: View
  where Modifier: ViewModifier
{
  public let modifier: Modifier
  public let view: AnyView

  public init(modifier: Modifier, view: AnyView) {
    self.modifier = modifier
    self.view = view
  }

  public var body: some View {
    view
  }
}

public extension View {
  func modifier<Modifier>(_ modifier: Modifier) -> ModifiedContent<Self, Modifier> {
    .init(content: self, modifier: modifier)
  }
}

public extension ViewModifier where Body == Never {
  func body(content: Content) -> Body {
    fatalError(
      "\(Self.self) is a primitive `ViewModifier`, you're not supposed to run `body(content:)`"
    )
  }
}
