/* GtkComponentPeer.java -- Implements ComponentPeer with GTK
   Copyright (C) 1998, 1999, 2002, 2004 Free Software Foundation, Inc.

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

import java.awt.AWTEvent;
import java.awt.BufferCapabilities;
import java.awt.Color;
import java.awt.Component;
import java.awt.Container;
import java.awt.Cursor;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.FontMetrics;
import java.awt.Frame;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.GraphicsConfiguration;
import java.awt.Image;
import java.awt.Insets;
import java.awt.ItemSelectable;
import java.awt.KeyboardFocusManager;
import java.awt.Point;
import java.awt.Rectangle;
import java.awt.Toolkit;
import java.awt.Window;
import java.awt.event.FocusEvent;
import java.awt.event.ItemEvent;
import java.awt.event.KeyEvent;
import java.awt.event.MouseEvent;
import java.awt.event.PaintEvent;
import java.awt.image.ColorModel;
import java.awt.image.ImageObserver;
import java.awt.image.ImageProducer;
import java.awt.image.VolatileImage;
import java.awt.peer.ComponentPeer;

public class GtkComponentPeer extends GtkGenericPeer
  implements ComponentPeer
{
  Component awtComponent;

  Insets insets;

  /* this isEnabled differs from Component.isEnabled, in that it
     knows if a parent is disabled.  In that case Component.isEnabled 
     may return true, but our isEnabled will always return false */
  native boolean isEnabled ();
  native static boolean modalHasGrab ();

  native int[] gtkWidgetGetForeground ();
  native int[] gtkWidgetGetBackground ();
  native void gtkWidgetSetVisible (boolean b);
  native void gtkWidgetGetDimensions (int[] dim);
  native void gtkWidgetGetPreferredDimensions (int[] dim);
  native void gtkWidgetGetLocationOnScreen (int[] point);
  native void gtkWidgetSetCursor (int type);
  native void gtkWidgetSetBackground (int red, int green, int blue);
  native void gtkWidgetSetForeground (int red, int green, int blue);
  native void gtkWidgetSetSensitive (boolean sensitive);
  native void gtkWidgetSetParent (ComponentPeer parent);
  native void gtkWidgetRequestFocus ();
  native void gtkWidgetDispatchKeyEvent (int id, long when, int mods,
                                         int keyCode, int keyLocation);
  native void gtkSetFont (String name, int style, int size);
  native void gtkWidgetQueueDrawArea(int x, int y, int width, int height);
  native void addExposeFilter();
  native void removeExposeFilter();

  void create ()
  {
    throw new RuntimeException ();
  }

  native void connectJObject ();
  native void connectSignals ();

  protected GtkComponentPeer (Component awtComponent)
  {
    super (awtComponent);
    this.awtComponent = awtComponent;
    insets = new Insets (0, 0, 0, 0);

    create ();

    setParent ();

    connectJObject ();
    connectSignals ();

    if (awtComponent.getForeground () != null)
      setForeground (awtComponent.getForeground ());
    if (awtComponent.getBackground () != null)
      setBackground (awtComponent.getBackground ());
    if (awtComponent.getFont() != null)
      setFont(awtComponent.getFont());

    setCursor (awtComponent.getCursor ());

    setComponentBounds ();

    Rectangle bounds = awtComponent.getBounds ();
    setBounds (bounds.x, bounds.y, bounds.width, bounds.height);
    setVisibleAndEnabled ();
  }

  void setParent ()
  {
    ComponentPeer p;
    Component component = awtComponent;
    do
      {
        component = component.getParent ();
        p = component.getPeer ();
      }
    while (p instanceof java.awt.peer.LightweightPeer);

    if (p != null)
      gtkWidgetSetParent (p);
  }

  /*
   * Set the bounds of this peer's AWT Component based on dimensions
   * returned by the native windowing system.  Most Components impose
   * their dimensions on the peers so the default implementation does
   * nothing.  However some peers, like GtkFileDialogPeer, need to
   * pass their size back to the AWT Component.
   */
  void setComponentBounds ()
  {
  }

  void setVisibleAndEnabled ()
  {
    setVisible (awtComponent.isVisible ());
    setEnabled (awtComponent.isEnabled ());
  }

  public int checkImage (Image image, int width, int height, 
			 ImageObserver observer) 
  {
    GtkImage i = (GtkImage) image;
    return i.checkImage ();
  }

  public Image createImage (ImageProducer producer) 
  {
    return new GtkImage (producer, null);
  }

  public Image createImage (int width, int height)
  {
    Graphics g;
    if (GtkToolkit.useGraphics2D ())
      {
        Graphics2D g2 = new GdkGraphics2D (width, height);
        g2.setBackground (getBackground ());
        g = g2;
      }
    else
      g = new GdkGraphics (width, height);

    return new GtkOffScreenImage (null, g, width, height);
  }

  public void disable () 
  {
    setEnabled (false);
  }

  public void enable () 
  {
    setEnabled (true);
  }

  public ColorModel getColorModel () 
  {
    return ColorModel.getRGBdefault ();
  }

  public FontMetrics getFontMetrics (Font font)
  {
    return new GdkFontMetrics (font);
  }

  public Graphics getGraphics ()
  {
    if (GtkToolkit.useGraphics2D ())
        return new GdkGraphics2D (this);
    else
        return new GdkGraphics (this);
  }

  public Point getLocationOnScreen () 
  { 
    int point[] = new int[2];
    gtkWidgetGetLocationOnScreen (point);
    return new Point (point[0], point[1]);
  }

  public Dimension getMinimumSize () 
  {
    return minimumSize ();
  }

  public Dimension getPreferredSize ()
  {
    return preferredSize ();
  }

  public Toolkit getToolkit ()
  {
    return Toolkit.getDefaultToolkit();
  }
  
  public void handleEvent (AWTEvent event)
  {
    int id = event.getID();
    KeyEvent ke = null;

    switch (id)
      {
      case PaintEvent.PAINT:
      case PaintEvent.UPDATE:
        {
          try 
            {
              Graphics g = getGraphics ();
          
              // Some peers like GtkFileDialogPeer are repainted by Gtk itself
              if (g == null)
                break;
		
              g.setClip (((PaintEvent)event).getUpdateRect());

              if (id == PaintEvent.PAINT)
                awtComponent.paint (g);
              else
                awtComponent.update (g);

              g.dispose ();
            }
          catch (InternalError e)
            {
              System.err.println (e);
            }
        }
        break;
      case KeyEvent.KEY_PRESSED:
        ke = (KeyEvent) event;
        gtkWidgetDispatchKeyEvent (ke.getID (), ke.getWhen (), ke.getModifiersEx (),
                                   ke.getKeyCode (), ke.getKeyLocation ());
        break;
      case KeyEvent.KEY_RELEASED:
        ke = (KeyEvent) event;
        gtkWidgetDispatchKeyEvent (ke.getID (), ke.getWhen (), ke.getModifiersEx (),
                                   ke.getKeyCode (), ke.getKeyLocation ());
        break;
      }
  }
  
  public boolean isFocusTraversable () 
  {
    return true;
  }

  public Dimension minimumSize () 
  {
    int dim[] = new int[2];

    gtkWidgetGetPreferredDimensions (dim);

    return new Dimension (dim[0], dim[1]);
  }

  public void paint (Graphics g)
  {
    Component parent = awtComponent.getParent();
    GtkComponentPeer parentPeer = null;
    if ((parent instanceof Container) && !parent.isLightweight())
      parentPeer = (GtkComponentPeer) parent.getPeer();

    addExposeFilter();
    if (parentPeer != null)
      parentPeer.addExposeFilter();

    Rectangle clip = g.getClipBounds();
    gtkWidgetQueueDrawArea(clip.x, clip.y, clip.width, clip.height);

    removeExposeFilter();
    if (parentPeer != null)
      parentPeer.removeExposeFilter();
  }

  public Dimension preferredSize ()
  {
    int dim[] = new int[2];

    gtkWidgetGetPreferredDimensions (dim);

    return new Dimension (dim[0], dim[1]);
  }

  public boolean prepareImage (Image image, int width, int height,
			       ImageObserver observer) 
  {
    GtkImage i = (GtkImage) image;

    if (i.isLoaded ()) return true;

    class PrepareImage extends Thread
    {
      GtkImage image;
      ImageObserver observer;

      PrepareImage (GtkImage image, ImageObserver observer)
      {
	this.image = image;
	image.setObserver (observer);
      }
      
      public void run ()
      {
	image.source.startProduction (image);
      }
    }

    new PrepareImage (i, observer).start ();
    return false;
  }

  public void print (Graphics g) 
  {
    throw new RuntimeException ();
  }

  public void repaint (long tm, int x, int y, int width, int height)
  {
    q.postEvent (new PaintEvent (awtComponent, PaintEvent.UPDATE,
				 new Rectangle (x, y, width, height)));
  }

  public void requestFocus ()
  {
    gtkWidgetRequestFocus();
    postFocusEvent(FocusEvent.FOCUS_GAINED, false);
  }

  public void reshape (int x, int y, int width, int height) 
  {
    setBounds (x, y, width, height);
  }

  public void setBackground (Color c) 
  {
    gtkWidgetSetBackground (c.getRed(), c.getGreen(), c.getBlue());
  }

  native public void setNativeBounds (int x, int y, int width, int height);

  public void setBounds (int x, int y, int width, int height)
  {
    Component parent = awtComponent.getParent ();

    // Heavyweight components that are children of one or more
    // lightweight containers have to be handled specially.  Because
    // calls to GLightweightPeer.setBounds do nothing, GTK has no
    // knowledge of the lightweight containers' positions.  So we have
    // to add the offsets manually when placing a heavyweight
    // component within a lightweight container.  The lightweight
    // container may itself be in a lightweight container and so on,
    // so we need to continue adding offsets until we reach a
    // container whose position GTK knows -- that is, the first
    // non-lightweight.
    boolean lightweightChild = false;
    Insets i;
    while (parent.isLightweight ())
      {
	lightweightChild = true;

	i = ((Container) parent).getInsets ();

	x += parent.getX () + i.left;
	y += parent.getY () + i.top;

	parent = parent.getParent ();
      }

    // We only need to convert from Java to GTK coordinates if we're
    // placing a heavyweight component in a Window.
    if (parent instanceof Window && !lightweightChild)
      {
	Insets insets = ((Window) parent).getInsets ();
	// Convert from Java coordinates to GTK coordinates.
	setNativeBounds (x - insets.left, y - insets.top, width, height);
      }
    else
      setNativeBounds (x, y, width, height);
  }

  public void setCursor (Cursor cursor) 
  {
    gtkWidgetSetCursor (cursor.getType ());
  }

  public void setEnabled (boolean b)
  {
    gtkWidgetSetSensitive (b);
  }

  public void setFont (Font f)
  {
    // FIXME: This should really affect the widget tree below me.
    // Currently this is only handled if the call is made directly on
    // a text widget, which implements setFont() itself.
    gtkSetFont(f.getName(), f.getStyle(), f.getSize());
  }

  public void setForeground (Color c) 
  {
    gtkWidgetSetForeground (c.getRed(), c.getGreen(), c.getBlue());
  }

  public Color getForeground ()
  {
    int rgb[] = gtkWidgetGetForeground ();
    return new Color (rgb[0], rgb[1], rgb[2]);
  }

  public Color getBackground ()
  {
    int rgb[] = gtkWidgetGetBackground ();
    return new Color (rgb[0], rgb[1], rgb[2]);
  }

  public void setVisible (boolean b)
  {
    if (b)
      show ();
    else
      hide ();
  }

  public native void hide ();
  public native void show ();

  protected void postMouseEvent(int id, long when, int mods, int x, int y, 
				int clickCount, boolean popupTrigger) 
  {
    q.postEvent(new MouseEvent(awtComponent, id, when, mods, x, y, 
			       clickCount, popupTrigger));
  }

  protected void postExposeEvent (int x, int y, int width, int height)
  {
    q.postEvent (new PaintEvent (awtComponent, PaintEvent.PAINT,
				 new Rectangle (x, y, width, height)));
  }

  protected void postKeyEvent (int id, long when, int mods,
                               int keyCode, char keyChar, int keyLocation)
  {
    KeyEvent keyEvent = new KeyEvent (awtComponent, id, when, mods,
                                      keyCode, keyChar, keyLocation);

    // Also post a KEY_TYPED event if keyEvent is a key press that
    // doesn't represent an action or modifier key.
    if (keyEvent.getID () == KeyEvent.KEY_PRESSED
        && (!keyEvent.isActionKey ()
            && keyCode != KeyEvent.VK_SHIFT
            && keyCode != KeyEvent.VK_CONTROL
            && keyCode != KeyEvent.VK_ALT))
      {
        synchronized (q)
          {
            q.postEvent (keyEvent);
            q.postEvent (new KeyEvent (awtComponent, KeyEvent.KEY_TYPED, when, mods,
                                        KeyEvent.VK_UNDEFINED, keyChar, keyLocation));
          }
      }
    else
      q.postEvent (keyEvent);
  }

  protected void postFocusEvent (int id, boolean temporary)
  {
    q.postEvent (new FocusEvent (awtComponent, id, temporary));
  }

  protected void postItemEvent (Object item, int stateChange)
  {
    q.postEvent (new ItemEvent ((ItemSelectable)awtComponent, 
				ItemEvent.ITEM_STATE_CHANGED,
				item, stateChange));
  }

  public GraphicsConfiguration getGraphicsConfiguration ()
  {
    // FIXME: just a stub for now.
    return null;
  }

  public void setEventMask (long mask)
  {
    // FIXME: just a stub for now.
  }

  public boolean isFocusable ()
  {
    return false;
  }

  public boolean requestFocus (Component source, boolean b1, 
                               boolean b2, long x)
  {
    return false;
  }

  public boolean isObscured ()
  {
    return false;
  }

  public boolean canDetermineObscurity ()
  {
    return false;
  }

  public void coalescePaintEvent (PaintEvent e)
  {
    
  }

  public void updateCursorImmediately ()
  {
    
  }

  public VolatileImage createVolatileImage (int width, int height)
  {
    return null;
  }

  public boolean handlesWheelScrolling ()
  {
    return false;
  }

  public void createBuffers (int x, BufferCapabilities capabilities)
    throws java.awt.AWTException

  {
    
  }

  public Image getBackBuffer ()
  {
    return null;
  }

  public void flip (BufferCapabilities.FlipContents contents)
  {
    
  }

  public void destroyBuffers ()
  {
    
  }
}
