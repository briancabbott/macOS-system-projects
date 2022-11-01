/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <agxbuf.h>

#define N_GNEW(n,t)	 (t*)malloc((n)*sizeof(t))

/* agxbinit:
 * Assume if init is non-null, hint = sizeof(init[])
 */
void
agxbinit (agxbuf* xb, unsigned int hint, unsigned char* init)
{
  if (init) {
    xb->buf = init;
    xb->dyna = 0;
  }
  else {
    if (hint == 0) hint = BUFSIZ;
    xb->dyna = 1;
    xb->buf = N_GNEW(hint,unsigned char);
  }
  xb->eptr = xb->buf + hint;
  xb->ptr = xb->buf;
  *xb->ptr = '\0';
}

/* agxbmore;
 * Expand buffer to hold at least ssz more bytes.
 */
int
agxbmore (agxbuf* xb, unsigned int ssz)
{
  int    cnt;         /* current no. of characters in buffer */
  int    size;        /* current buffer size */
  int    nsize;       /* new buffer size */
  unsigned char*  nbuf;        /* new buffer */

  size = xb->eptr - xb->buf;
  nsize = 2*size;
  if (size+ssz > nsize) nsize = size+ssz;
  cnt = xb->ptr - xb->buf;
  if (xb->dyna) {
    nbuf = realloc(xb->buf,nsize);
  }
  else {
    nbuf = N_GNEW(nsize,unsigned char);
    memcpy (nbuf, xb->buf, cnt);
    xb->dyna = 1;
  }
  xb->buf = nbuf;
  xb->ptr = xb->buf + cnt;
  xb->eptr = xb->buf + nsize;
  return 0;
}

/* agxbput_n:
 * Append string s of length n onto xb
 */
int
agxbput_n (agxbuf* xb, char* s, unsigned int ssz)
{
  if (xb->ptr + ssz > xb->eptr)
    agxbmore (xb, ssz);
  memcpy (xb->ptr, s, ssz);
  xb->ptr += ssz;
  return ssz;
}

/* agxbput:
 * Append string s into xb
 */
int
agxbput (agxbuf* xb, char* s)
{
  unsigned int    ssz = strlen (s);

  return agxbput_n(xb,s,ssz);
}

/* agxbfree:
 * Free any malloced resources.
 */
void
agxbfree (agxbuf* xb)
{
  if (xb->dyna) free (xb->buf);
}

/* agxbpop:
 * Removes last character added, if any.
 */
int
agxbpop (agxbuf* xb)
{
  int c;
  if (xb->ptr > xb->buf) {
    c = *xb->ptr--;
    return c;
  }
  else return -1;

}

