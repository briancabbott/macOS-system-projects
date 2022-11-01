#include "rs6000/xm-rs6000.h"

#undef USG
#define HAVE_STRERROR

/* bsd4_4 probably doesn't need to be defined if HAVE_STRERROR is defined.  */
#define bsd4_4

/* malloc does better with chunks the size of a page.  */ 

#define OBSTACK_CHUNK_SIZE (getpagesize ())
