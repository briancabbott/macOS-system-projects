#define SWIFT_NAME(X) __attribute__((swift_name(#X)))

#ifndef SWIFT_ENUM_EXTRA
#  define SWIFT_ENUM_EXTRA
#endif

#ifndef SWIFT_ENUM
#  define SWIFT_ENUM(_type, _name)    \
  enum _name : _type _name;           \
  enum SWIFT_ENUM_EXTRA _name : _type
#endif

// Renaming global variables.
int SNFoo SWIFT_NAME(Bar);

// Renaming tags and fields.
struct SWIFT_NAME(SomeStruct) SNSomeStruct {
  double X SWIFT_NAME(x);
};

// Renaming C functions
struct SNSomeStruct SNMakeSomeStruct(double X, double Y) SWIFT_NAME(makeSomeStruct(x:y:));

struct SNSomeStruct SNMakeSomeStructForX(double X) SWIFT_NAME(makeSomeStruct(x:));

// Renaming typedefs.
typedef int SNIntegerType SWIFT_NAME(MyInt);

// Renaming enumerations.
SWIFT_ENUM(unsigned char, SNColorChoice) {
  SNColorRed SWIFT_NAME(Rouge),
  SNColorGreen,
  SNColorBlue
};

// swift_private attribute
void SNTransposeInPlace(struct SNSomeStruct *value) __attribute__((swift_private));

typedef struct {
  double x, y, z;
} SNPoint SWIFT_NAME(Point);
