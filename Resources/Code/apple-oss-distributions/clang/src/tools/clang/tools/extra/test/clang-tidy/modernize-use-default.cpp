// RUN: %check_clang_tidy %s modernize-use-default %t -- -- -std=c++11 -fno-delayed-template-parsing  -fexceptions

// Out of line definition.
class OL {
public:
  OL();
  ~OL();
};

OL::OL() {}
// CHECK-MESSAGES: :[[@LINE-1]]:1: warning: use '= default' to define a trivial default constructor [modernize-use-default]
// CHECK-FIXES: OL::OL() = default;
OL::~OL() {}
// CHECK-MESSAGES: :[[@LINE-1]]:1: warning: use '= default' to define a trivial destructor [modernize-use-default]
// CHECK-FIXES: OL::~OL() = default;

// Inline definitions.
class IL {
public:
  IL() {}
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: use '= default'
  // CHECK-FIXES: IL() = default;
  ~IL() {}
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: use '= default'
  // CHECK-FIXES: ~IL() = default;
};

// Non-empty body.
void f();
class NE {
public:
  NE() { f(); }
  ~NE() { f(); }
};

// Initializer or arguments.
class IA {
public:
  // Constructor with initializer.
  IA() : Field(5) {}
  // Constructor with arguments.
  IA(int Arg1, int Arg2) {}
  int Field;
};

// Private constructor/destructor.
class Priv {
  Priv() {}
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: use '= default'
  // CHECK-FIXES: Priv() = default;
  ~Priv() {}
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: use '= default'
  // CHECK-FIXES: ~Priv() = default;
};

// struct.
struct ST {
  ST() {}
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: use '= default'
  // CHECK-FIXES: ST() = default;
  ~ST() {}
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: use '= default'
  // CHECK-FIXES: ST() = default;
};

// Deleted constructor/destructor.
class Del {
public:
  Del() = delete;
  ~Del() = delete;
};

// Do not remove other keywords.
class KW {
public:
  explicit KW() {}
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: use '= default'
  // CHECK-FIXES: explicit KW() = default;
  virtual ~KW() {}
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: use '= default'
  // CHECK-FIXES: virtual ~KW() = default;
};

// Nested class.
struct N {
  struct NN {
    NN() {}
    // CHECK-MESSAGES: :[[@LINE-1]]:5: warning: use '= default'
    // CHECK-FIXES: NN() = default;
    ~NN() {}
    // CHECK-MESSAGES: :[[@LINE-1]]:5: warning: use '= default'
    // CHECK-FIXES: ~NN() = default;
  };
  int Int;
};

// Class template.
template <class T>
class Temp {
public:
  Temp() {}
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: use '= default'
  // CHECK-FIXES: Temp() = default;
  ~Temp() {}
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: use '= default'
  // CHECK-FIXES: ~Temp() = default;
};

// Non user-provided constructor/destructor.
struct Imp {
  int Int;
};
void g() {
  Imp *PtrImp = new Imp();
  PtrImp->~Imp();
  delete PtrImp;
}

// Already using default.
struct IDef {
  IDef() = default;
  ~IDef() = default;
};
struct ODef {
  ODef();
  ~ODef();
};
ODef::ODef() = default;
ODef::~ODef() = default;

// Delegating constructor and overriden destructor.
struct DC : KW {
  DC() : KW() {}
  ~DC() override {}
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: use '= default'
  // CHECK-FIXES: ~DC() override = default;
};

struct Comments {
  Comments() {
    // Don't erase comments inside the body.
  }
  ~Comments() {
    // Don't erase comments inside the body.
  }
};

// Try-catch.
struct ITC {
  ITC() try {} catch(...) {}
  ~ITC() try {} catch(...) {}
};

struct OTC {
  OTC();
  ~OTC();
};
OTC::OTC() try {} catch(...) {}
OTC::~OTC() try {} catch(...) {}
