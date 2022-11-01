/* APPLE LOCAL method encoding */
/* Method encoding tests for stand-alone @protocol declarations.  */
/* Contributed by Ziemowit Laski <zlaski@apple.com>.  */
/* { dg-options "-lobjc" } */
/* { dg-do run } */

#import <objc/Protocol.h>
#ifdef __cplusplus
#define ProtoBool bool
#else
#define ProtoBool _Bool
#endif

extern int sscanf(const char *str, const char *format, ...);
extern void abort(void);
#define CHECK_IF(expr) if(!(expr)) abort()

enum Enum {
  zero, one, two, three
};
typedef enum Enum Enum;

@protocol Proto
union __XXAngle { unsigned int alpha, beta; };
typedef struct { float x, y; union __XXAngle a; } XXPoint;
typedef struct { double width, height; } XXSize;
typedef struct _XXRect { XXPoint origin; XXSize size; struct _XXRect *next; } XXRect;
- (void) char:(char)c float:(float)f double:(double)d unsigned:(unsigned)u short:(short)s long:(long)l;
- (void *)setRect:(XXRect)r withBool:(ProtoBool)b withInt:(int)i;
+ (Enum *)getEnum:(XXPoint *)pt enum:(enum Enum)e bool:(BOOL)b;
+ (ProtoBool **)getBool:(BOOL **)b;
@end

Protocol *proto = @protocol(Proto);
struct objc_method_description *meth;
unsigned totsize, offs0, offs1, offs2, offs3, offs4, offs5, offs6, offs7;

static void scan_initial(const char *pattern) {
  totsize = offs0 = offs1 = offs2 = offs3 = offs4 = offs5 = offs6 = offs7 = (unsigned)-1;
  sscanf(meth->types, pattern, &totsize, &offs0, &offs1, &offs2, &offs3,
      &offs4, &offs5, &offs6, &offs7);
  CHECK_IF(!offs0 && offs1 == sizeof(id) && offs2 == offs1 + sizeof(SEL) && totsize >= offs2);
}

int main(void) {
  meth = [proto descriptionForInstanceMethod: @selector(char:float:double:unsigned:short:long:)];
  scan_initial("v%u@%u:%uc%uf%ud%uI%us%ul%u");
  CHECK_IF(offs3 == offs2 + sizeof(int) && offs4 == offs3 + sizeof(float));
  CHECK_IF(offs5 == offs4 + sizeof(double) && offs6 == offs5 + sizeof(unsigned));
  CHECK_IF(offs7 == offs6 + sizeof(int) && totsize == offs7 + sizeof(long));
  meth = [proto descriptionForInstanceMethod: @selector(setRect:withBool:withInt:)];
  scan_initial("^v%u@%u:%u{_XXRect={?=ff(__XXAngle=II)}{?=dd}^{_XXRect}}%uB%ui%u");
  CHECK_IF(offs3 == offs2 + sizeof(XXRect) && offs4 == offs3 + sizeof(ProtoBool));
  CHECK_IF(totsize == offs4 + sizeof(int));
  meth = [proto descriptionForClassMethod: @selector(getEnum:enum:bool:)];
  scan_initial("^i%u@%u:%u^{?=ff(__XXAngle=II)}%ui%uc%u");
  CHECK_IF(offs3 == offs2 + sizeof(XXPoint *) && offs4 == offs3 + sizeof(enum Enum));
  CHECK_IF(totsize == offs4 + sizeof(int));  /* 'BOOL' is really 'char' */
  meth = [proto descriptionForClassMethod: @selector(getBool:)];         
  scan_initial("^^B%u@%u:%u^^c%u");
  CHECK_IF(totsize == offs2 + sizeof(BOOL **));
  return 0;
}
