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

import TokamakCore

extension _BackgroundStyleModifier: DOMViewModifier {
  public var isOrderDependent: Bool { true }
  public var attributes: [HTMLAttribute: String] {
    if let resolved = style.resolve(
      for: .resolveStyle(levels: 0..<1),
      in: environment,
      role: .fill
    ) {
      if case let .foregroundMaterial(rgba, material) = resolved {
        let blur: (opacity: Double, radius: Double)
        switch material {
        case .ultraThin:
          blur = (0.2, 20)
        case .thin:
          blur = (0.4, 25)
        case .regular:
          blur = (0.5, 30)
        case .thick:
          blur = (0.6, 40)
        case .ultraThick:
          blur = (0.6, 50)
        }
        return [
          "style":
            """
            background-color: rgba(\(rgba.red * 255), \(rgba.green * 255), \(rgba
              .blue * 255), \(blur
              .opacity));
            -webkit-backdrop-filter: blur(\(blur.radius)px);
            backdrop-filter: blur(\(blur.radius)px);
            """,
        ]
      } else if let color = resolved.color(at: 0) {
        return [
          "style": "background-color: \(color.cssValue(environment));",
        ]
      }
    }
    return [:]
  }
}
