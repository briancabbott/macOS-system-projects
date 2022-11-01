/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/
/* $XFree86: xc/lib/GL/dri/dri_glx.c,v 1.12 2003/02/06 12:42:10 alanh Exp $ */

/*
 * Authors:
 *   Kevin E. Martin <kevin@precisioninsight.com>
 *   Brian Paul <brian@precisioninsight.com>
 *
 */

#ifdef GLX_DIRECT_RENDERING

#include <unistd.h>
#include <X11/Xlibint.h>
#include <X11/extensions/Xext.h>
#include "extutil.h"
#include "glxclient.h"
#include "xf86dri.h"
#include "sarea.h"
#include <stdio.h>
#include <dlfcn.h>
#include "dri_glx.h"
#include <sys/types.h>
#include <stdarg.h>


typedef void *(*CreateScreenFunc)(Display *dpy, int scrn, __DRIscreen *psc,
                                  int numConfigs, __GLXvisualConfig *config);

typedef void *(*RegisterExtensionsFunc)(void);


#ifdef BUILT_IN_DRI_DRIVER

extern void *__driCreateScreen(Display *dpy, int scrn, __DRIscreen *psc,
                               int numConfigs, __GLXvisualConfig *config);


#else /* BUILT_IN_DRI_DRIVER */


#ifndef DEFAULT_DRIVER_DIR
/* this is normally defined in the Imakefile */
#define DEFAULT_DRIVER_DIR "/usr/X11R6/lib/modules/dri"
#endif

/*
 * We keep a linked list of these structures, one per DRI device driver.
 */
typedef struct __DRIdriverRec {
   const char *name;
   void *handle;
   CreateScreenFunc createScreenFunc;
   RegisterExtensionsFunc registerExtensionsFunc;
   struct __DRIdriverRec *next;
} __DRIdriver;

static __DRIdriver *Drivers = NULL;


/*
 * printf wrappers
 */

static void InfoMessageF(const char *f, ...)
{
    va_list args;
    const char *env;

    if ((env = getenv("LIBGL_DEBUG")) && strstr(env, "verbose")) {
	fprintf(stderr, "libGL: ");
	va_start(args, f);
	vfprintf(stderr, f, args);
	va_end(args);
    }
}

static void ErrorMessageF(const char *f, ...)
{
    va_list args;

    if (getenv("LIBGL_DEBUG")) {
	fprintf(stderr, "libGL error: ");
	va_start(args, f);
	vfprintf(stderr, f, args);
	va_end(args);
    }
}


/*
 * We'll save a pointer to this function when we couldn't find a
 * direct rendering driver for a given screen.
 */
static void *DummyCreateScreen(Display *dpy, int scrn, __DRIscreen *psc,
                               int numConfigs, __GLXvisualConfig *config)
{
    (void) dpy;
    (void) scrn;
    (void) psc;
    (void) numConfigs;
    (void) config;
    return NULL;
}



/*
 * Extract the ith directory path out of a colon-separated list of
 * paths.
 * Input:
 *   index - index of path to extract (starting at zero)
 *   paths - the colon-separated list of paths
 *   dirLen - max length of result to store in <dir>
 * Output:
 *   dir - the extracted directory path, dir[0] will be zero when
 *         extraction fails.
 */
static void ExtractDir(int index, const char *paths, int dirLen, char *dir)
{
   int i, len;
   const char *start, *end;

   /* find ith colon */
   start = paths;
   i = 0;
   while (i < index) {
      if (*start == ':') {
         i++;
         start++;
      }
      else if (*start == 0) {
         /* end of string and couldn't find ith colon */
         dir[0] = 0;
         return;
      }
      else {
         start++;
      }
   }

   while (*start == ':')
      start++;

   /* find next colon, or end of string */
   end = start + 1;
   while (*end != ':' && *end != 0) {
      end++;
   }

   /* copy string between <start> and <end> into result string */
   len = end - start;
   if (len > dirLen - 1)
      len = dirLen - 1;
   strncpy(dir, start, len);
   dir[len] = 0;
}


/*
 * Try to dlopen() the named driver.  This function adds the
 * "_dri.so" suffix to the driver name and searches the
 * directories specified by the LIBGL_DRIVERS_PATH env var
 * in order to find the driver.
 * Input:
 *   driverName - a name like "tdfx", "i810", "mga", etc.
 * Return:
 *   handle from dlopen, or NULL if driver file not found.
 */
static __DRIdriver *OpenDriver(const char *driverName)
{
   char *libPaths = NULL;
   int i;
   __DRIdriver *driver;

   /* First, search Drivers list to see if we've already opened this driver */
   for (driver = Drivers; driver; driver = driver->next) {
      if (strcmp(driver->name, driverName) == 0) {
         /* found it */
         return driver;
      }
   }

   if (geteuid() == getuid()) {
      /* don't allow setuid apps to use LIBGL_DRIVERS_PATH */
      libPaths = getenv("LIBGL_DRIVERS_PATH");
      if (!libPaths)
         libPaths = getenv("LIBGL_DRIVERS_DIR"); /* deprecated */
   }
   if (!libPaths)
      libPaths = DEFAULT_DRIVER_DIR;

   for (i = 0; ; i++) {
      char libDir[1000], realDriverName[200];
      void *handle;
      ExtractDir(i, libPaths, 1000, libDir);
      if (!libDir[0])
         break; /* ran out of paths to search */
      snprintf(realDriverName, 200, "%s/%s_dri.so", libDir, driverName);
      InfoMessageF("OpenDriver: trying %s\n", realDriverName);
      handle = dlopen(realDriverName, RTLD_NOW | RTLD_GLOBAL);
      if (handle) {
         /* allocate __DRIdriver struct */
         driver = (__DRIdriver *) Xmalloc(sizeof(__DRIdriver));
         if (!driver)
            return NULL; /* out of memory! */
         /* init the struct */
         driver->name = __glXstrdup(driverName);
         if (!driver->name) {
            Xfree(driver);
            return NULL; /* out of memory! */
         }
         driver->createScreenFunc = (CreateScreenFunc)
            dlsym(handle, "__driCreateScreen");
         if (!driver->createScreenFunc) {
            /* If the driver doesn't have this symbol then something's
             * really, really wrong.
             */
            ErrorMessageF("__driCreateScreen() not defined in %s_dri.so!\n",
                          driverName);
            Xfree(driver);
            dlclose(handle);
            continue;
         }
         driver->registerExtensionsFunc = (RegisterExtensionsFunc)
            dlsym(handle, "__driRegisterExtensions");
         driver->handle = handle;
         /* put at head of linked list */
         driver->next = Drivers;
         Drivers = driver;
         return driver;
      }
      else {
	 ErrorMessageF("dlopen %s failed (%s)\n", realDriverName, dlerror());
      }
   }

   ErrorMessageF("unable to find driver: %s_dri.so\n", driverName);
   return NULL;
}


/*
 * Given a display pointer and screen number, determine the name of
 * the DRI driver for the screen. (I.e. "r128", "tdfx", etc).
 * Return True for success, False for failure.
 */
static Bool GetDriverName(Display *dpy, int scrNum, char **driverName)
{
   int directCapable;
   Bool b;
   int driverMajor, driverMinor, driverPatch;

   *driverName = NULL;

   if (!XF86DRIQueryDirectRenderingCapable(dpy, scrNum, &directCapable)) {
      ErrorMessageF("XF86DRIQueryDirectRenderingCapable failed");
      return False;
   }
   if (!directCapable) {
      ErrorMessageF("XF86DRIQueryDirectRenderingCapable returned false");
      return False;
   }

   b = XF86DRIGetClientDriverName(dpy, scrNum, &driverMajor, &driverMinor,
                                  &driverPatch, driverName);
   if (!b) {
      ErrorMessageF("Cannot determine driver name for screen %d\n", scrNum);
      return False;
   }

   InfoMessageF("XF86DRIGetClientDriverName: %d.%d.%d %s (screen %d)\n",
	     driverMajor, driverMinor, driverPatch, *driverName, scrNum);

   return True;
}


/*
 * Given a display pointer and screen number, return a __DRIdriver handle.
 * Return NULL if anything goes wrong.
 */
static __DRIdriver *GetDriver(Display *dpy, int scrNum)
{
   char *driverName;
   __DRIdriver *ret;

   if (GetDriverName(dpy, scrNum, &driverName)) {
      ret = OpenDriver(driverName);
      if (driverName)
     	 Xfree(driverName);
      return ret;
   }

   return NULL;
}


#endif /* BUILT_IN_DRI_DRIVER */


/* This function isn't currently used.
 */
static void driDestroyDisplay(Display *dpy, void *private)
{
    __DRIdisplayPrivate *pdpyp = (__DRIdisplayPrivate *)private;

    if (pdpyp) {
        const int numScreens = ScreenCount(dpy);
        int i;
        for (i = 0; i < numScreens; i++) {
            if (pdpyp->libraryHandles[i])
                dlclose(pdpyp->libraryHandles[i]);
        }
        Xfree(pdpyp->libraryHandles);
	Xfree(pdpyp);
    }
}


/*
 * Allocate, initialize and return a __DRIdisplayPrivate object.
 * This is called from __glXInitialize() when we are given a new
 * display pointer.
 */
void *driCreateDisplay(Display *dpy, __DRIdisplay *pdisp)
{
    const int numScreens = ScreenCount(dpy);
    __DRIdisplayPrivate *pdpyp;
    int eventBase, errorBase;
    int major, minor, patch;
    int scrn;

    /* Initialize these fields to NULL in case we fail.
     * If we don't do this we may later get segfaults trying to free random
     * addresses when the display is closed.
     */
    pdisp->private = NULL;
    pdisp->destroyDisplay = NULL;
    pdisp->createScreen = NULL;

    if (!XF86DRIQueryExtension(dpy, &eventBase, &errorBase)) {
	return NULL;
    }

    if (!XF86DRIQueryVersion(dpy, &major, &minor, &patch)) {
	return NULL;
    }

    pdpyp = (__DRIdisplayPrivate *)Xmalloc(sizeof(__DRIdisplayPrivate));
    if (!pdpyp) {
	return NULL;
    }

    pdpyp->driMajor = major;
    pdpyp->driMinor = minor;
    pdpyp->driPatch = patch;

    pdisp->destroyDisplay = driDestroyDisplay;

    /* allocate array of pointers to createScreen funcs */
    pdisp->createScreen = (CreateScreenFunc *) Xmalloc(numScreens * sizeof(void *));
    if (!pdisp->createScreen) {
       XFree(pdpyp);
       return NULL;
    }

    /* allocate array of library handles */
    pdpyp->libraryHandles = (void **) Xmalloc(numScreens * sizeof(void*));
    if (!pdpyp->libraryHandles) {
       Xfree(pdisp->createScreen);
       XFree(pdpyp);
       return NULL;
    }

#ifdef BUILT_IN_DRI_DRIVER
    /* we'll statically bind to the built-in __driCreateScreen function */
    for (scrn = 0; scrn < numScreens; scrn++) {
       pdisp->createScreen[scrn] = __driCreateScreen;
       pdpyp->libraryHandles[scrn] = NULL;
    }

#else
    /* dynamically discover DRI drivers for all screens, saving each
     * driver's "__driCreateScreen" function pointer.  That's the bootstrap
     * entrypoint for all DRI drivers.
     */
    __glXRegisterExtensions();
    for (scrn = 0; scrn < numScreens; scrn++) {
        __DRIdriver *driver = GetDriver(dpy, scrn);
        if (driver) {
           pdisp->createScreen[scrn] = driver->createScreenFunc;
           pdpyp->libraryHandles[scrn] = driver->handle;
        }
        else {
           pdisp->createScreen[scrn] = DummyCreateScreen;
           pdpyp->libraryHandles[scrn] = NULL;
        }
    }
#endif

    return (void *)pdpyp;
}



/*
** Here we'll query the DRI driver for each screen and let each
** driver register its GL extension functions.  We only have to
** do this once.  But it MUST be done before we create any contexts
** (i.e. before any dispatch tables are created) and before
** glXGetProcAddressARB() returns.
**
** Currently called by glXGetProcAddress(), __glXInitialize(), and
** __glXNewIndirectAPI().
*/
void
__glXRegisterExtensions(void)
{
#ifndef BUILT_IN_DRI_DRIVER
   static GLboolean alreadyCalled = GL_FALSE;
   int displayNum, maxDisplays;

   if (alreadyCalled)
      return;
   alreadyCalled = GL_TRUE;

   if (getenv("LIBGL_MULTIHEAD")) {
      /* we'd like to always take this path but doing so causes a second
       * or more of delay while the XOpenDisplay() function times out.
       */
      maxDisplays = 10;  /* infinity, really */
   }
   else {
      /* just open the :0 display */
      maxDisplays = 1;
   }

   for (displayNum = 0; displayNum < maxDisplays; displayNum++) {
      char displayName[200];
      Display *dpy;
      snprintf(displayName, 199, ":%d.0", displayNum);
      dpy = XOpenDisplay(displayName);
      if (dpy) {
         const int numScreens = ScreenCount(dpy);
         int screenNum;
         for (screenNum = 0; screenNum < numScreens; screenNum++) {
            __DRIdriver *driver = GetDriver(dpy, screenNum);
            if (driver && driver->registerExtensionsFunc) {
               (*driver->registerExtensionsFunc)();
            }
         }
         XCloseDisplay(dpy);
      }
      else {
         break;
      }
   }
#endif
}


#endif /* GLX_DIRECT_RENDERING */
