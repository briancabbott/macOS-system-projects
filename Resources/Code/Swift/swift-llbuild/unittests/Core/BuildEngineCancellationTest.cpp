//===- unittests/Core/BuildEngineCancellationTest.cpp ---------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2017-2018 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#include "llbuild/Core/BuildEngine.h"

#include "llbuild/Basic/ExecutionQueue.h"
#include "llbuild/Core/BuildDB.h"

#include "llvm/ADT/StringMap.h"
#include "llvm/Support/ErrorHandling.h"

#include "gtest/gtest.h"

#include <unordered_map>
#include <vector>

using namespace llbuild;
using namespace llbuild::core;

namespace {

class SimpleBuildEngineDelegate : public core::BuildEngineDelegate, public basic::ExecutionQueueDelegate {
public:
  bool expectError = false;
private:
  virtual std::unique_ptr<core::Rule> lookupRule(const core::KeyType& key) override {
    // We never expect dynamic rule lookup.
    fprintf(stderr, "error: unexpected rule lookup for \"%s\"\n",
            key.c_str());
    abort();
    return nullptr;
  }

  virtual void cycleDetected(const std::vector<core::Rule*>& items) override {
    error("unexpected cycle detected");
  }

  virtual void error(const Twine& message) override {
    fprintf(stderr, "error: %s\n", message.str().c_str());
    if (!expectError)
      abort();
  }

  void processStarted(basic::ProcessContext*, basic::ProcessHandle, llbuild_pid_t) override { }
  void processHadError(basic::ProcessContext*, basic::ProcessHandle, const Twine&) override { }
  void processHadOutput(basic::ProcessContext*, basic::ProcessHandle, StringRef) override { }
  void processFinished(basic::ProcessContext*, basic::ProcessHandle, const basic::ProcessResult&) override { }
  void queueJobStarted(basic::JobDescriptor*) override { }
  void queueJobFinished(basic::JobDescriptor*) override { }

  std::unique_ptr<basic::ExecutionQueue> createExecutionQueue() override {
    return createSerialQueue(*this, nullptr);
  }
};

static int32_t intFromValue(const core::ValueType& value) {
  assert(value.size() == 4);
  return ((value[0] << 0) |
          (value[1] << 8) |
          (value[2] << 16) |
          (value[3] << 24));
}
static core::ValueType intToValue(int32_t value) {
  std::vector<uint8_t> result(4);
  result[0] = (value >> 0) & 0xFF;
  result[1] = (value >> 8) & 0xFF;
  result[2] = (value >> 16) & 0xFF;
  result[3] = (value >> 24) & 0xFF;
  return result;
}

// Simple task implementation which takes a fixed set of dependencies, evaluates
// them all, and then provides the output.
class SimpleTask : public Task {
public:
  typedef std::function<std::vector<KeyType>()> InputListingFnType;
  typedef std::function<int(const std::vector<int>&)> ComputeFnType;

private:
  InputListingFnType listInputs;
  std::vector<int> inputValues;
  ComputeFnType compute;

public:
  SimpleTask(InputListingFnType listInputs, ComputeFnType compute)
      : listInputs(listInputs), compute(compute)
  {
  }

  virtual void start(TaskInterface ti) override {
    // Compute the list of inputs.
    auto inputs = listInputs();

    // Request all of the inputs.
    inputValues.resize(inputs.size());
    for (int i = 0, e = inputs.size(); i != e; ++i) {
      ti.request(inputs[i], i);
    }
  }

  virtual void provideValue(TaskInterface, uintptr_t inputID,
                            const ValueType& value) override {
    // Update the input values.
    assert(inputID < inputValues.size());
    inputValues[inputID] = intFromValue(value);
  }

  virtual void inputsAvailable(TaskInterface ti) override {
    ti.complete(intToValue(compute(inputValues)));
  }
};

// Helper function for creating a simple action.
typedef std::function<Task*(BuildEngine&)> ActionFn;


class SimpleRule: public Rule {
public:
  typedef std::function<bool(const ValueType& value)> ValidFnType;

private:
  SimpleTask::ComputeFnType compute;
  std::vector<KeyType> inputs;
public:
  SimpleRule(const KeyType& key, const std::vector<KeyType>& inputs,
             SimpleTask::ComputeFnType compute)
    : Rule(key), compute(compute), inputs(inputs) { }

  Task* createTask(BuildEngine&) override {
    return new SimpleTask([this]{ return inputs; }, compute);
  }

  bool isResultValid(BuildEngine&, const ValueType&) override { return true; }
};



TEST(BuildEngineCancellationTest, basic) {
  std::vector<std::string> builtKeys;
  SimpleBuildEngineDelegate delegate;
  core::BuildEngine engine(delegate);
  bool cancelIt = false;
  engine.addRule(std::unique_ptr<core::Rule>(new SimpleRule(
      "value-A", {}, [&] (const std::vector<int>& inputs) {
          builtKeys.push_back("value-A");
          fprintf(stderr, "building A (and cancelling ? %d)\n", cancelIt);
          if (cancelIt) {
            engine.cancelBuild();
          }
          return 2; })));
  engine.addRule(std::unique_ptr<core::Rule>(new SimpleRule(
      "result", {"value-A"},
                   [&] (const std::vector<int>& inputs) {
                     EXPECT_EQ(1U, inputs.size());
                     EXPECT_EQ(2, inputs[0]);
                     builtKeys.push_back("result");
                     return inputs[0] * 3;
                   })));

  // Build the result, cancelling during the first task.
  cancelIt = true;
  auto result = engine.build("result");
  EXPECT_EQ(0U, result.size());
  EXPECT_EQ(1U, builtKeys.size());
  EXPECT_EQ("value-A", builtKeys[0]);

  // Build again, without cancelling; both tasks should run.
  engine.resetForBuild();
  cancelIt = false;
  EXPECT_EQ(2 * 3, intFromValue(engine.build("result")));
  EXPECT_EQ(2U, builtKeys.size());
  EXPECT_EQ("value-A", builtKeys[0]);
  EXPECT_EQ("result", builtKeys[1]);
}

}
