/* GdkGraphics.java
   Copyright (C) 1998, 1999, 2002 Free Software Foundation, Inc.

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

import java.awt.Color;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.FontMetrics;
import java.awt.Graphics;
import java.awt.Image;
import java.awt.Rectangle;
import java.awt.Shape;
import java.awt.SystemColor;
import java.awt.image.ImageObserver;
import java.text.AttributedCharacterIterator;

public class GdkGraphics extends Graphics
{
  private final int native_state = GtkGenericPeer.getUniqueInteger();

  Color color, xorColor;
  GtkComponentPeer component;
  Font font;
  Rectangle clip;

  int xOffset = 0;
  int yOffset = 0;

  static final int GDK_COPY = 0, GDK_XOR = 2;

  native void initState (GtkComponentPeer component);
  native void initState (int width, int height);
  native void copyState (GdkGraphics g);

  GdkGraphics (GdkGraphics g)
  {
    color = g.color;
    xorColor = g.xorColor;
    font = g.font;
    clip = new Rectangle (g.clip);
    component = g.component;

    copyState (g);
  }

  GdkGraphics (int width, int height)
  {
    initState (width, height);
    color = Color.black;
    clip = new Rectangle (0, 0, width, height);
    font = new Font ("Dialog", Font.PLAIN, 12);
  }

  GdkGraphics (GtkComponentPeer component)
  {
    this.component = component;
    initState (component);
    color = component.awtComponent.getForeground ();
    font = component.awtComponent.getFont ();
    Dimension d = component.awtComponent.getSize ();
    clip = new Rectangle (0, 0, d.width, d.height);
  }

  public native void clearRect (int x, int y, int width, int height);

  public void clipRect (int x, int y, int width, int height)
  {
    clip = clip.intersection (new Rectangle (x, y, width, height));
    setClipRectangle (clip.x, clip.y, clip.width, clip.height);
  }

  native public void copyArea (int x, int y, int width, int height, 
			       int dx, int dy);

  public Graphics create ()
  {
    return new GdkGraphics (this);
  }

//    public Graphics create (int x, int y, int width, int height)
//    {
//      GdkGraphics g = new GdkGraphics (this);
//      System.out.println ("translating by: " + x +" " + y);
//      g.translate (x, y);
//      g.clipRect (0, 0, width, height);

//      return g;
//    }
  
  native public void dispose ();

  native void copyPixmap (Graphics g, int x, int y, int width, int height);
  native void copyAndScalePixmap (Graphics g, boolean flip_x, boolean flip_y,
                                  int src_x, int src_y, 
                                  int src_width, int src_height, 
                                  int dest_x, int dest_y, 
                                  int dest_width, int dest_height);
  public boolean drawImage (Image img, int x, int y, 
			    Color bgcolor, ImageObserver observer)
  {
    if (img instanceof GtkOffScreenImage)
      {
	copyPixmap (img.getGraphics (), 
		    x, y, img.getWidth (null), img.getHeight (null));
	return true;
      }

    GtkImage image = (GtkImage) img;
    new GtkImagePainter (image, this, x, y, -1, -1, bgcolor);
    return image.isLoaded ();
  }

  public boolean drawImage (Image img, int x, int y, ImageObserver observer)
  {
    if (img instanceof GtkOffScreenImage)
      {
	copyPixmap (img.getGraphics (), 
		    x, y, img.getWidth (null), img.getHeight (null));
	return true;
      }

    if (component != null)
      return drawImage (img, x, y, component.getBackground (), observer);
    else
      return drawImage (img, x, y, SystemColor.window, observer);
  }

  public boolean drawImage (Image img, int x, int y, int width, int height, 
			    Color bgcolor, ImageObserver observer)
  {
    if (img instanceof GtkOffScreenImage)
      {
        copyAndScalePixmap (img.getGraphics (), false, false,
                            0, 0, img.getWidth (null), img.getHeight (null), 
                            x, y, width, height);
        return true;
      }

    GtkImage image = (GtkImage) img;
    new GtkImagePainter (image, this, x, y, width, height, bgcolor);
    return image.isLoaded ();
  }

  public boolean drawImage (Image img, int x, int y, int width, int height, 
			    ImageObserver observer)
  {
    if (component != null)
      return drawImage (img, x, y, width, height, component.getBackground (),
                        observer);
    else
      return drawImage (img, x, y, width, height, SystemColor.window,
                        observer);
  }

  public boolean drawImage (Image img, int dx1, int dy1, int dx2, int dy2, 
			    int sx1, int sy1, int sx2, int sy2, 
			    Color bgcolor, ImageObserver observer)
  {
    if (img instanceof GtkOffScreenImage)
      {
        int dx_start, dy_start, d_width, d_height;
        int sx_start, sy_start, s_width, s_height;
        boolean x_flip = false;
        boolean y_flip = false;

        if (dx1 < dx2)
        {
          dx_start = dx1;
          d_width = dx2 - dx1;
        }
        else
        {
          dx_start = dx2;
          d_width = dx1 - dx2;
          x_flip ^= true;
        }
        if (dy1 < dy2)
        {
          dy_start = dy1;
          d_height = dy2 - dy1;
        }
        else
        {
          dy_start = dy2;
          d_height = dy1 - dy2;
          y_flip ^= true;
        }
        if (sx1 < sx2)
        {
          sx_start = sx1;
          s_width = sx2 - sx1;
        }
        else
        {
          sx_start = sx2;
          s_width = sx1 - sx2;
          x_flip ^= true;
        }
        if (sy1 < sy2)
        {
          sy_start = sy1;
          s_height = sy2 - sy1;
        }
        else
        {
          sy_start = sy2;
          s_height = sy1 - sy2;
          y_flip ^= true;
        }

        copyAndScalePixmap (img.getGraphics (), x_flip, y_flip,
                            sx_start, sy_start, s_width, s_height, 
                            dx_start, dy_start, d_width, d_height);
        return true;
      }

    GtkImage image = (GtkImage) img;
    new GtkImagePainter (image, this, dx1, dy1, dx2, dy2, 
			 sx1, sy1, sx2, sy2, bgcolor);
    return image.isLoaded ();
  }

  public boolean drawImage (Image img, int dx1, int dy1, int dx2, int dy2, 
			    int sx1, int sy1, int sx2, int sy2, 
			    ImageObserver observer) 
  {
    if (component != null)
      return drawImage (img, dx1, dy1, dx2, dy2, sx1, sy1, sx2, sy2,
                        component.getBackground (), observer);
    else
      return drawImage (img, dx1, dy1, dx2, dy2, sx1, sy1, sx2, sy2,
                        SystemColor.window, observer);
  }

  native public void drawLine (int x1, int y1, int x2, int y2);

  native public void drawArc (int x, int y, int width, int height,
			      int startAngle, int arcAngle);
  native public void fillArc (int x, int y, int width, int height, 
			      int startAngle, int arcAngle);
  native public void drawOval(int x, int y, int width, int height);
  native public void fillOval(int x, int y, int width, int height);

  native public void drawPolygon(int[] xPoints, int[] yPoints, int nPoints);
  native public void fillPolygon(int[] xPoints, int[] yPoints, int nPoints);

  native public void drawPolyline(int[] xPoints, int[] yPoints, int nPoints);

  native public void drawRect(int x, int y, int width, int height);
  native public void fillRect (int x, int y, int width, int height);

  native void drawString (String str, int x, int y, String fname, int style, int size);
  public void drawString (String str, int x, int y)
  {
    drawString (str, x, y, font.getName(), font.getStyle(), font.getSize());
  }

  public void drawString (AttributedCharacterIterator ci, int x, int y)
  {
    throw new Error ("not implemented");
  }

  public void drawRoundRect(int x, int y, int width, int height, 
			    int arcWidth, int arcHeight)
  {
    if (arcWidth > width)
      arcWidth = width;
    if (arcHeight > height)
      arcHeight = height;

    int xx = x + width - arcWidth;
    int yy = y + height - arcHeight;

    drawArc (x, y, arcWidth, arcHeight, 90, 90);
    drawArc (xx, y, arcWidth, arcHeight, 0, 90);
    drawArc (xx, yy, arcWidth, arcHeight, 270, 90);
    drawArc (x, yy, arcWidth, arcHeight, 180, 90);

    int y1 = y + arcHeight / 2;
    int y2 = y + height - arcHeight / 2;
    drawLine (x, y1, x, y2);
    drawLine (x + width, y1, x + width, y2);

    int x1 = x + arcWidth / 2;
    int x2 = x + width - arcWidth / 2;
    drawLine (x1, y, x2, y);
    drawLine (x1, y + height, x2, y + height);
  }

  public void fillRoundRect (int x, int y, int width, int height, 
			     int arcWidth, int arcHeight)
  {
    if (arcWidth > width)
      arcWidth = width;
    if (arcHeight > height)
      arcHeight = height;

    int xx = x + width - arcWidth;
    int yy = y + height - arcHeight;

    fillArc (x, y, arcWidth, arcHeight, 90, 90);
    fillArc (xx, y, arcWidth, arcHeight, 0, 90);
    fillArc (xx, yy, arcWidth, arcHeight, 270, 90);
    fillArc (x, yy, arcWidth, arcHeight, 180, 90);

    fillRect (x, y + arcHeight / 2, width, height - arcHeight + 1);
    fillRect (x + arcWidth / 2, y, width - arcWidth + 1, height);
  }

  public Shape getClip ()
  {
    return getClipBounds ();
  }

  public Rectangle getClipBounds ()
  {
//      System.out.println ("returning CLIP: " + clip);
    return new Rectangle (clip.x, clip.y, clip.width, clip.height);
  }

  public Color getColor ()
  {
    return color;
  }

  public Font getFont ()
  {
    return font;
  }

  public FontMetrics getFontMetrics (Font font)
  {
    return new GdkFontMetrics (font);
  }

  native void setClipRectangle (int x, int y, int width, int height);

  public void setClip (int x, int y, int width, int height)
  {
    clip.x = x;
    clip.y = y;
    clip.width = width;
    clip.height = height;
    
    setClipRectangle (x, y, width, height);
  }

  public void setClip (Rectangle clip)
  {
    setClip (clip.x, clip.y, clip.width, clip.height);
  }

  public void setClip (Shape clip)
  {
    setClip (clip.getBounds ());
  }

  native private void setFGColor (int red, int green, int blue);

  public void setColor (Color c)
  {
    if (c == null)
      color = new Color (0, 0, 0);
    else
      color = c;

    if (xorColor == null) /* paint mode */
      setFGColor (color.getRed (), color.getGreen (), color.getBlue ());
    else		  /* xor mode */
      setFGColor (color.getRed   () ^ xorColor.getRed (),
		  color.getGreen () ^ xorColor.getGreen (),
		  color.getBlue  () ^ xorColor.getBlue ());
  }

  public void setFont (Font font)
  {
    this.font = font;
  }

  native void setFunction (int gdk_func);

  public void setPaintMode ()
  {
    xorColor = null;

    setFunction (GDK_COPY);
    setFGColor (color.getRed (), color.getGreen (), color.getBlue ());
  }

  public void setXORMode (Color c)
  {
    xorColor = c;

    setFunction (GDK_XOR);
    setFGColor (color.getRed   () ^ xorColor.getRed (),
		color.getGreen () ^ xorColor.getGreen (),
		color.getBlue  () ^ xorColor.getBlue ());
  }

  native public void translateNative (int x, int y);

  public void translate (int x, int y)
  {
    clip.x -= x;
    clip.y -= y;

    translateNative (x, y);
  }
}
