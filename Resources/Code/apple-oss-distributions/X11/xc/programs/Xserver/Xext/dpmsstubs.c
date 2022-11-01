/* $Xorg: dpmsstubs.c,v 1.3 2000/08/17 19:47:56 cpqbld Exp $ */
/*****************************************************************

Copyright (c) 1996 Digital Equipment Corporation, Maynard, Massachusetts.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
DIGITAL EQUIPMENT CORPORATION BE LIABLE FOR ANY CLAIM, DAMAGES, INCLUDING, 
BUT NOT LIMITED TO CONSEQUENTIAL OR INCIDENTAL DAMAGES, OR OTHER LIABILITY, 
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR 
IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Digital Equipment Corporation 
shall not be used in advertising or otherwise to promote the sale, use or other
dealings in this Software without prior written authorization from Digital 
Equipment Corporation.

******************************************************************/
/* $XFree86: xc/programs/Xserver/Xext/dpmsstubs.c,v 3.4 2001/01/17 22:13:15 dawes Exp $ */

typedef int Bool;

#define FALSE 0

Bool DPMSSupported(void)
{
    return FALSE;
}

int DPSMGet(int *level)
{
    return -1;
}

void DPMSSet(int level)
{

}
