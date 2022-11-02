// Copyright 2021 Tokamak contributors
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

#if compiler(>=5.5) && (canImport(Concurrency) || canImport(_Concurrency))

public extension View {
  func task(
    priority: TaskPriority = .userInitiated,
    _ action: @escaping @Sendable () async -> ()
  ) -> some View {
    var task: Task<(), Never>?
    return onAppear {
      task = Task(priority: priority, operation: action)
    }
    .onDisappear {
      task?.cancel()
    }
  }
}

#endif
