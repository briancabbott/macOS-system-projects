/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.iiop.rmi;

import org.omg.CORBA.Any;


/**
 *  Constant analysis.
 *
 *  Routines here are conforming to the "Java(TM) Language to IDL Mapping
 *  Specification", version 1.1 (01-06-07).
 *      
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.2 $
 */
public class ConstantAnalysis
   extends AbstractAnalysis
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   ConstantAnalysis(String javaName, Class type, Object value)
   {
      super(javaName);

      if (type == Void.TYPE ||
          !type.isPrimitive() && type != java.lang.String.class) 
         throw new IllegalArgumentException("Bad type for constant: " +
                                            type.getName());

      this.type = type;
      this.value = value;
   }

   // Public --------------------------------------------------------

   /**
    *  Return my Java type.
    */
   public Class getType()
   {
      return type;
   }
   
   /**
    *  Return my value.
    */
   public Object getValue()
   {
      return value;
   }
   
   /**
    *  Insert the constant value into the argument Any.
    */
   public void insertValue(Any any)
   {
      if (type == String.class)
         any.insert_wstring((String)value); // 1.3.5.10 Map to wstring
      else
         Util.insertAnyPrimitive(any, value);
   }

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   /**
    *  Java type of constant.
    */
   private Class type;

   /**
    *  The value of the constant.
    */
   private Object value;

}

