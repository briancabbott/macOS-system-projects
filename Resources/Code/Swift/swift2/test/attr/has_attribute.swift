// RUN: %target-typecheck-verify-swift

#if !hasAttribute(dynamicCallable)
BOOM
#endif

#if hasAttribute(fortran)
BOOM
#endif

#if hasAttribute(cobol)
this is unparsed junk // expected-error{{consecutive statements on a line must be separated by}}
#endif

#if hasAttribute(optional)
ModifiersAreNotAttributes
#endif

#if hasAttribute(__raw_doc_comment)
UserInaccessibleAreNotAttributes
#endif

#if hasAttribute(17)
// expected-error@-1{{unexpected platform condition argument: expected attribute name}}
#endif
