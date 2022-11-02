#ifndef TEST_INTEROP_CXX_CLASS_INPUTS_PROTOCOL_CONFORMANCE_H
#define TEST_INTEROP_CXX_CLASS_INPUTS_PROTOCOL_CONFORMANCE_H

struct ConformsToProtocol {
  int return42() { return 42; }
};

struct DoesNotConformToProtocol {
  int returnFortyTwo() { return 42; }
};

struct DummyStruct {};

struct __attribute__((swift_attr("import_unsafe"))) NonTrivial {
  ~NonTrivial() {}
  NonTrivial(DummyStruct) {}
  NonTrivial() {}
  void test1() {}
  void test2(int) {}
  char test3(int, unsigned) { return 42; }
};

struct Trivial {
  Trivial(DummyStruct) {}
  Trivial() {}
  void test1() {}
  void test2(int) {}
  char test3(int, unsigned) { return 42; }
};

struct ReturnsNullableValue {
  const int *returnPointer() __attribute__((swift_attr("import_unsafe"))) {
    return nullptr;
  }
};

struct ReturnsNonNullValue {
  const int *returnPointer() __attribute__((returns_nonnull))
  __attribute__((swift_attr("import_unsafe"))) {
    return (int *)this;
  }
};

struct HasOperatorExclaim {
  int value;

  HasOperatorExclaim operator!() const { return {-value}; }
};

struct HasOperatorEqualEqual {
  int value;
  
  bool operator==(const HasOperatorEqualEqual &other) const {
    return value == other.value;
  }
};

#endif // TEST_INTEROP_CXX_CLASS_INPUTS_PROTOCOL_CONFORMANCE_H