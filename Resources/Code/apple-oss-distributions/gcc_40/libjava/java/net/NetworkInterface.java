/* NetworkInterface.java --
   Copyright (C) 2002, 2003 Free Software Foundation, Inc.

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


package java.net;

import gnu.classpath.Configuration;

import java.util.Enumeration;
import java.util.Vector;

/**
 * This class models a network interface on the host computer.  A network
 * interface contains a name (typically associated with a specific
 * hardware adapter) and a list of addresses that are bound to it.
 * For example, an ethernet interface may be named "eth0" and have the
 * address 192.168.1.101 assigned to it.
 *
 * @author Michael Koch (konqueror@gmx.de)
 * @since 1.4
 */
public final class NetworkInterface
{
  static
    {
      if (Configuration.INIT_LOAD_LIBRARY)
	System.loadLibrary("javanet");
    }

  private String name;
  private Vector inetAddresses;

  private NetworkInterface(String name, InetAddress address)
  {
    this.name = name;
    this.inetAddresses = new Vector(1, 1);
    this.inetAddresses.add(address);
  }

  private static native Vector getRealNetworkInterfaces()
    throws SocketException;

  /**
   * Returns the name of the network interface
   *
   * @return The name of the interface.
   */
  public String getName()
  {
    return name;
  }

  /**
   *  Returns all available addresses of the network interface
   *
   *  If a @see SecurityManager is available all addresses are checked
   *  with @see SecurityManager::checkConnect() if they are available.
   *  Only <code>InetAddresses</code> are returned where the security manager
   *  doesn't throw an exception.
   *
   *  @return An enumeration of all addresses.
   */
  public Enumeration getInetAddresses()
  {
    SecurityManager s = System.getSecurityManager();

    if (s == null)
      return inetAddresses.elements();

    Vector tmpInetAddresses = new Vector(1, 1);

    for (Enumeration addresses = inetAddresses.elements();
         addresses.hasMoreElements();)
      {
	InetAddress addr = (InetAddress) addresses.nextElement();
	try
	  {
	    s.checkConnect(addr.getHostAddress(), 58000);
	    tmpInetAddresses.add(addr);
	  }
	catch (SecurityException e)
	  {
	    // Ignore.
	  }
      }

    return tmpInetAddresses.elements();
  }

  /**
   *  Returns the display name of the interface
   *
   *  @return The display name of the interface
   */
  public String getDisplayName()
  {
    return name;
  }

  /**
   * Returns an network interface by name
   *
   * @param name The name of the interface to return
   * 
   * @return a <code>NetworkInterface</code> object representing the interface,
   * or null if there is no interface with that name.
   *
   * @exception SocketException If an error occurs
   * @exception NullPointerException If the specified name is null
   */
  public static NetworkInterface getByName(String name)
    throws SocketException
  {
    Vector networkInterfaces = getRealNetworkInterfaces();

    for (Enumeration e = networkInterfaces.elements(); e.hasMoreElements();)
      {
	NetworkInterface tmp = (NetworkInterface) e.nextElement();

	if (name.equals(tmp.getName()))
	  return tmp;
      }

    // No interface with the given name found.
    return null;
  }

  /**
   *  Return a network interface by its address
   *
   *  @param addr The address of the interface to return
   *
   *  @exception SocketException If an error occurs
   *  @exception NullPointerException If the specified addess is null
   */
  public static NetworkInterface getByInetAddress(InetAddress addr)
    throws SocketException
  {
    Vector networkInterfaces = getRealNetworkInterfaces();

    for (Enumeration interfaces = networkInterfaces.elements();
         interfaces.hasMoreElements();)
      {
	NetworkInterface tmp = (NetworkInterface) interfaces.nextElement();

	for (Enumeration addresses = tmp.inetAddresses.elements();
	     addresses.hasMoreElements();)
	  {
	    if (addr.equals((InetAddress) addresses.nextElement()))
	      return tmp;
	  }
      }

    throw new SocketException("no network interface is bound to such an IP address");
  }

  /**
   *  Return an <code>Enumeration</code> of all available network interfaces
   *
   *  @exception SocketException If an error occurs
   */
  public static Enumeration getNetworkInterfaces() throws SocketException
  {
    Vector networkInterfaces = getRealNetworkInterfaces();

    if (networkInterfaces.isEmpty())
      return null;

    return networkInterfaces.elements();
  }

  /**
   *  Checks if the current instance is equal to obj
   *
   *  @param obj The object to compare with
   */
  public boolean equals(Object obj)
  {
    if (! (obj instanceof NetworkInterface))
      return false;

    NetworkInterface tmp = (NetworkInterface) obj;

    return (name.equals(tmp.name) && inetAddresses.equals(tmp.inetAddresses));
  }

  /**
   *  Returns the hashcode of the current instance
   */
  public int hashCode()
  {
    // FIXME: hash correctly
    return name.hashCode() + inetAddresses.hashCode();
  }

  /**
   *  Returns a string representation of the interface
   */
  public String toString()
  {
    // FIXME: check if this is correct
    String result;
    String separator = System.getProperty("line.separator");

    result =
      "name: " + getDisplayName() + " (" + getName() + ") addresses:"
      + separator;

    for (Enumeration e = inetAddresses.elements(); e.hasMoreElements();)
      {
	InetAddress address = (InetAddress) e.nextElement();
	result += address.toString() + ";" + separator;
      }

    return result;
  }
} // class NetworkInterface
