/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.util.xml;

import org.w3c.dom.Element;


/**
 *   <description> 
 *      
 *   @see <related>
 *   @author <a href="mailto:sebastien.alborini@m4x.org">Sebastien Alborini</a>
 *   @version $Revision: 1.3 $
 */
public interface XmlLoadable
{
    
   // Public --------------------------------------------------------
   public void importXml(Element element);
}
