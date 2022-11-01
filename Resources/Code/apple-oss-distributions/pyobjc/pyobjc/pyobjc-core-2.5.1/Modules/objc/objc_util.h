#ifndef OBJC_UTIL
#define OBJC_UTIL

#include <Foundation/NSException.h>
#define THREADSTATE_AUTORELEASEPOOL "__threadstate_autoreleasepool"

extern PyObject* PyObjCExc_Error;
extern PyObject* PyObjCExc_NoSuchClassError;
extern PyObject* PyObjCExc_InternalError;
extern PyObject* PyObjCExc_UnInitDeallocWarning;
extern PyObject* PyObjCExc_ObjCRevivalWarning;
extern PyObject* PyObjCExc_LockError;
extern PyObject* PyObjCExc_BadPrototypeError;
extern PyObject* PyObjCExc_UnknownPointerError;

int PyObjCUtil_Init(PyObject* module);

void PyObjCErr_FromObjC(NSException* localException);
void PyObjCErr_ToObjC(void) __attribute__((__noreturn__));

void PyObjCErr_ToObjCWithGILState(PyGILState_STATE* state) __attribute__((__noreturn__));

NSException* PyObjCErr_AsExc(void);

PyObject* PyObjC_CallPython(id self, SEL selector, PyObject* arglist, BOOL* isAlloc, BOOL* isCFAlloc);

char* PyObjCUtil_Strdup(const char* value);

#include <Foundation/NSMapTable.h>
extern NSMapTableKeyCallBacks PyObjCUtil_PointerKeyCallBacks;
extern NSMapTableValueCallBacks PyObjCUtil_PointerValueCallBacks;

extern NSMapTableKeyCallBacks PyObjCUtil_ObjCIdentityKeyCallBacks;
extern NSMapTableValueCallBacks PyObjCUtil_ObjCValueCallBacks;

void    PyObjC_FreeCArray(int, void*);
int     PyObjC_PythonToCArray(BOOL, BOOL, const char*, PyObject*, void**, Py_ssize_t*, PyObject**);
PyObject* PyObjC_CArrayToPython(const char*, void*, Py_ssize_t);
PyObject* PyObjC_CArrayToPython2(const char*, void*, Py_ssize_t, bool already_retained, bool already_cfretained);
int     PyObjC_IsPythonKeyword(const char* word);


extern int PyObjCRT_SimplifySignature(const char* signature, char* buf, size_t buflen);

extern int PyObjCObject_Convert(PyObject* object, void* pvar);
extern int PyObjCClass_Convert(PyObject* object, void* pvar);

extern int PyObjC_is_ascii_string(PyObject* unicode_string, const char* ascii_string);
extern int PyObjC_is_ascii_prefix(PyObject* unicode_string, const char* ascii_string, size_t n);

extern PyObject* PyObjC_ImportName(const char* name);

#endif /* OBJC_UTIL */
