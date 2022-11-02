// Copyright 2020-2021 Tokamak contributors
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
//
//  Created by Carson Katri on 6/29/20.
//

import Foundation

public extension InsettableShape {
  func strokeBorder<S>(
    _ content: S,
    style: StrokeStyle,
    antialiased: Bool = true
  ) -> some View where S: ShapeStyle {
    inset(by: style.lineWidth / 2)
      .stroke(style: style)
      .fill(content, style: FillStyle(antialiased: antialiased))
  }

  @inlinable
  func strokeBorder(style: StrokeStyle, antialiased: Bool = true) -> some View {
    inset(by: style.lineWidth / 2)
      .stroke(style: style)
      .fill(style: FillStyle(antialiased: antialiased))
  }

  @inlinable
  func strokeBorder<S>(
    _ content: S,
    lineWidth: CGFloat = 1,
    antialiased: Bool = true
  ) -> some View where S: ShapeStyle {
    strokeBorder(
      content,
      style: StrokeStyle(lineWidth: lineWidth),
      antialiased: antialiased
    )
  }

  @inlinable
  func strokeBorder(lineWidth: CGFloat = 1, antialiased: Bool = true) -> some View {
    strokeBorder(
      style: StrokeStyle(lineWidth: lineWidth),
      antialiased: antialiased
    )
  }
}
