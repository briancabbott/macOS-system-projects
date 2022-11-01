.. title:: clang-tidy - readability-implicit-bool-cast

readability-implicit-bool-cast
==============================

This check can be used to find implicit conversions between built-in types and
booleans. Depending on use case, it may simply help with readability of the code,
or in some cases, point to potential bugs which remain unnoticed due to implicit
conversions.

The following is a real-world example of bug which was hiding behind implicit
bool cast:

.. code:: c++

  class Foo {
    int m_foo;
  public:
    void setFoo(bool foo) { m_foo = foo; } // warning: implicit cast bool -> int
    int getFoo() { return m_foo; }
  };

  void use(Foo& foo) {
    bool value = foo.getFoo(); // warning: implicit cast int -> bool
  }

This code is the result of unsuccessful refactoring, where type of ``m_foo``
changed from ``bool`` to ``int``. The programmer forgot to change all
occurences of ``bool``, and the remaining code is no longer correct, yet it
still compiles without any visible warnings.

In addition to issuing warnings, FixIt hints are provided to help solve
the reported issues. This can be used for improving readabilty of code, for example:

.. code:: c++

  void conversionsToBool() {
    float floating;
    bool boolean = floating;
    // ^ propose replacement: bool boolean = floating != 0.0f;

    int integer;
    if (integer) {}
    // ^ propose replacement: if (integer != 0) {}

    int* pointer;
    if (!pointer) {}
    // ^ propose replacement: if (pointer == nullptr) {}

    while (1) {}
    // ^ propose replacement: while (true) {}
  }

  void functionTakingInt(int param);

  void conversionsFromBool() {
    bool boolean;
    functionTakingInt(boolean);
    // ^ propose replacement: functionTakingInt(static_cast<int>(boolean));

    functionTakingInt(true);
    // ^ propose replacement: functionTakingInt(1);
  }

In general, the following cast types are checked:
 - integer expression/literal to boolean,
 - floating expression/literal to boolean,
 - pointer/pointer to member/``nullptr``/``NULL`` to boolean,
 - boolean expression/literal to integer,
 - boolean expression/literal to floating.

The rules for generating FixIt hints are:
 - in case of casts from other built-in type to bool, an explicit comparison
   is proposed to make it clear what exaclty is being compared:

   - ``bool boolean = floating;`` is changed to ``bool boolean = floating == 0.0f;``,
   - for other types, appropriate literals are used (``0``, ``0u``, ``0.0f``, ``0.0``, ``nullptr``),
 - in case of negated expressions cast to bool, the proposed replacement with
   comparison is simplified:

   - ``if (!pointer)`` is changed to ``if (pointer == nullptr)``,
 - in case of casts from bool to other built-in types, an explicit ``static_cast``
   is proposed to make it clear that a cast is taking place:

   - ``int integer = boolean;`` is changed to ``int integer = static_cast<int>(boolean);``,
 - if the cast is performed on type literals, an equivalent literal is proposed,
   according to what type is actually expected, for example:

   - ``functionTakingBool(0);`` is changed to ``functionTakingBool(false);``,
   - ``functionTakingInt(true);`` is changed to ``functionTakingInt(1);``,
   - for other types, appropriate literals are used (``false``, ``true``, ``0``, ``1``, ``0u``, ``1u``,
     ``0.0f``, ``1.0f``, ``0.0``, ``1.0f``).

Some additional accommodations are made for pre-C++11 dialects:
 - ``false`` literal cast to pointer is detected,
 - instead of ``nullptr`` literal, ``0`` is proposed as replacement.

Occurences of implicit casts inside macros and template instantiations are
deliberately ignored, as it is not clear how to deal with such cases.
