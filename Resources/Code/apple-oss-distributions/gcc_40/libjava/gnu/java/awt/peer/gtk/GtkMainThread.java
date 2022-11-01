/* GtkMainThread.java -- Runs gtk_main()
   Copyright (C) 1998, 1999 Free Software Foundation, Inc.

This file is part of GNU Classpath.

GNU Classpath is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Classpath is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Classpath; see the file COPYING.  If not, write to the
Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
02111-1307 USA.

Linking this library statically or dynamically with other modules is
making a combined work based on this library.  Thus, the terms and
conditions of the GNU General Public License cover the whole
combination.

As a special exception, the copyright holders of this library give you
permission to link this library with independent modules to produce an
executable, regardless of the license terms of these independent
modules, and to copy and distribute the resulting executable under
terms of your choice, provided that you also meet, for each linked
independent module, the terms and conditions of the license of that
module.  An independent module is a module which is not derived from
or based on this library.  If you modify this library, you may extend
this exception to your version of the library, but you are not
obligated to do so.  If you do not wish to do so, delete this
exception statement from your version. */


package gnu.java.awt.peer.gtk;

public class GtkMainThread extends GtkGenericPeer implements Runnable
{
  private static Thread mainThread = null;
  private static Object mainThreadLock = new Object();

  // Whether the gtk+ subsystem has been initialized.
  private boolean gtkInitCalled = false;

  /**
   * Call gtk_init.  It is very important that this happen before any other
   * gtk calls.
   *
   * @param portableNativeSync 1 if the Java property
   * gnu.classpath.awt.gtk.portable.native.sync is set to "true".  0 if it is
   * set to "false".  -1 if unset.
   */
  static native void gtkInit(int portableNativeSync);
  native void gtkMain();
  
  public GtkMainThread() 
  {
    super (null);
    synchronized (mainThreadLock) 
      {
	if (mainThread != null)
	  throw new IllegalStateException();
	mainThread = new Thread(this, "GtkMain");
      }
    
    synchronized (this) 
      {
	mainThread.start();
	
	while (!gtkInitCalled)
	  {
	    try
	      {
		wait();
	      }
	    catch (InterruptedException e) { }
	  }
      }
  }
  
  public void run() 
  {
    /* Pass the value of the gnu.classpath.awt.gtk.portable.native.sync system
     * property to C. */ 
    int portableNativeSync;     
    String portNatSyncProp = 
      System.getProperty("gnu.classpath.awt.gtk.portable.native.sync");

    if (portNatSyncProp == null)
      portableNativeSync = -1;  // unset
    else if (Boolean.valueOf(portNatSyncProp).booleanValue())
      portableNativeSync = 1;   // true
    else
      portableNativeSync = 0;   // false
    
    synchronized (this) 
      {
	gtkInit(portableNativeSync);
	gtkInitCalled = true;
	notifyAll();
      }
    gtkMain();
  }
}



