/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.iiop.rmi.ir;

import org.omg.CORBA.Repository;
import org.omg.CORBA.RepositoryPOATie;
import org.omg.CORBA.ORB;
import org.omg.CORBA.Any;
import org.omg.CORBA.IRObject;
import org.omg.CORBA.Container;
import org.omg.CORBA.DefinitionKind;
import org.omg.CORBA.RepositoryOperations;
import org.omg.CORBA.Contained;
import org.omg.CORBA.ContainedHelper;
import org.omg.CORBA.TypeCode;
import org.omg.CORBA.TCKind;
import org.omg.CORBA.PrimitiveDef;
import org.omg.CORBA.StringDef;
import org.omg.CORBA.WstringDef;
import org.omg.CORBA.SequenceDef;
import org.omg.CORBA.InterfaceDef;
import org.omg.CORBA.ArrayDef;
import org.omg.CORBA.FixedDef;
import org.omg.CORBA.BAD_INV_ORDER;
import org.omg.PortableServer.POA;
import org.omg.PortableServer.POAHelper;

import java.util.Map;
import java.util.HashMap;
import java.util.Iterator;

import java.io.UnsupportedEncodingException;


/**
 *  An Interface Repository.
 *
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.5 $
 */
class RepositoryImpl
   extends ContainerImpl
   implements RepositoryOperations, LocalContainer
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   private static final org.jboss.logging.Logger logger = 
               org.jboss.logging.Logger.getLogger(RepositoryImpl.class);

   // Constructors --------------------------------------------------

   public RepositoryImpl(ORB orb, POA poa, String name)
   {
      super(DefinitionKind.dk_Repository, null);

      this.orb = orb;
      this.poa = poa;
      try {
         oid = (name).getBytes("UTF-8");
      } catch (UnsupportedEncodingException ex) {
         throw new RuntimeException("UTF-8 encoding not supported.");
      }
      oidPrefix = name + ":";
      anonOidPrefix = oidPrefix + "anon";

      repository = this;
   }

   // Public --------------------------------------------------------

   // LocalIRObject implementation ----------------------------------
 
   public IRObject getReference()
   {
      if (ref == null) {
         ref = org.omg.CORBA.RepositoryHelper.narrow(
                              servantToReference(new RepositoryPOATie(this)) );
      }
      return ref;
   }

   public void allDone()
      throws IRConstructionException
   {
      super.allDone();

      // call allDone() for all our sequences
      Iterator iter = sequenceMap.values().iterator();
      while (iter.hasNext())
         ((SequenceDefImpl)iter.next()).allDone();
   }

   public void shutdown()
   {
      // shutdown all anonymous IR objects in this IR
      for (long i = 1; i < nextPOAId; i++) {
         try {
            getPOA().deactivate_object(getAnonymousObjectId(i));
         } catch (org.omg.CORBA.UserException ex) {
            logger.warn("Could not deactivate anonymous IR object", ex);
         }
      } 
      
      // shutdown this IR's top-level container
      super.shutdown();
   }

   // Repository implementation -------------------------------------

   public Contained lookup_id(java.lang.String search_id)
   {
      logger.debug("RepositoryImpl.lookup_id(\"" + search_id + "\") entered.");
      LocalContained c = _lookup_id(search_id);

      if (c == null)
         return null;

      return ContainedHelper.narrow(c.getReference());
   }

   public TypeCode get_canonical_typecode(org.omg.CORBA.TypeCode tc)
   {
      logger.debug("RepositoryImpl.get_canonical_typecode() entered.");
      // TODO
      return null;
   }

   public PrimitiveDef get_primitive(org.omg.CORBA.PrimitiveKind kind)
   {
      logger.debug("RepositoryImpl.get_primitive() entered.");
      // TODO
      return null;
   }

   public StringDef create_string(int bound)
   {
      throw new BAD_INV_ORDER("Cannot change RMI/IIOP mapping.");
   }

   public WstringDef create_wstring(int bound)
   {
      throw new BAD_INV_ORDER("Cannot change RMI/IIOP mapping.");
   }

   public SequenceDef create_sequence(int bound, org.omg.CORBA.IDLType element_type)
   {
      throw new BAD_INV_ORDER("Cannot change RMI/IIOP mapping.");
   }

   public ArrayDef create_array(int length, org.omg.CORBA.IDLType element_type)
   {
      throw new BAD_INV_ORDER("Cannot change RMI/IIOP mapping.");
   }

   public FixedDef create_fixed(short digits, short scale)
   {
      throw new BAD_INV_ORDER("Cannot change RMI/IIOP mapping.");
   }

 

   // Y overrides ---------------------------------------------------

   // Package protected ---------------------------------------------

   /**
    *  The ORB that I use.
    */
   ORB orb = null;

   /**
    *  The POA that I use.
    */
   POA poa = null;

   /**
    *  The POA object ID of this repository.
    */
   private byte[] oid = null;

   /**
    *  Prefix for POA object IDs of IR objects in this repository.
    */
   private String oidPrefix = null;

   /**
    *  Prefix for POA object IDs of "anonymous" IR objects in this repository.
    */
   private String anonOidPrefix = null;

   /**
    *  Maps typecodes of sequences defined in this IR to the sequences.
    */
   Map sequenceMap = new HashMap();

   /**
    *  Maps repository IDs of sequences defined in this IR to the sequences.
    */
   Map sequenceIdMap = new HashMap();


   LocalContained _lookup_id(java.lang.String search_id)
   {
      logger.debug("RepositoryImpl._lookup_id(\"" + search_id + 
                   "\") entered.");
      // mapping of arrays are special
      if (search_id.startsWith("RMI:["))
         return (ValueBoxDefImpl)sequenceIdMap.get(search_id);

      // convert id
      String name = scopedName(search_id);
      logger.debug("RepositoryImpl._lookup_id(): scopedName=\"" + 
                   scopedName(search_id) + "\"");

      // look it up if converted id not null
      //return (name == null) ? null : _lookup(name);
      LocalContained ret = (name == null) ? null : _lookup(name);
      logger.debug("RepositoryImpl._lookup_id(): returning " +
                   ((ret == null) ? "null" : "NOT null") );
      return ret;
   }

   SequenceDefImpl getSequenceImpl(TypeCode typeCode)
   {
      return (SequenceDefImpl)sequenceMap.get(typeCode);
   }

   void putSequenceImpl(String id, TypeCode typeCode, SequenceDefImpl sequence,
                        ValueBoxDefImpl valueBox)
   {
      sequenceIdMap.put(id, valueBox);
      sequenceMap.put(typeCode, sequence);
   }

   String getObjectIdPrefix()
   {
      return oidPrefix;
   }

   // Protected -----------------------------------------------------

   /**
    *  Return the POA object ID of this IR object.
    */
   protected byte[] getObjectId()
   {
      return (byte[])oid.clone();
   }

   /**
    *  Generate the ID of the n-th "anonymous" object created in this IR.
    */
   protected byte[] getAnonymousObjectId(long n)
   {
      String s = anonOidPrefix + Long.toString(n);
      try {
         return s.getBytes("UTF-8");
      } catch (UnsupportedEncodingException ex) {
         throw new RuntimeException("UTF-8 encoding not supported.");
      }
   }

   /**
    *  The next "anonymous" POA object ID.
    *  While contained IR objects can generate a sensible ID from their
    *  repository ID, non-contained objects use this method to get an
    *  ID that is unique within the IR.
    */
   protected byte[] getNextObjectId()
   {
      return getAnonymousObjectId(nextPOAId++);
   }


   // Private -------------------------------------------------------

   /**
    *  My CORBA reference.
    */
   private Repository ref = null;

   /**
    *  The next "anonymous" POA object ID.
    */
   private long nextPOAId = 1;

   /**
    *  Convert a repository ID to an IDL scoped name.
    *  Returns <code>null</code> if the ID cannot be understood.
    */
   private String scopedName(String id)
   {
      if (id == null)
         return null;

      if (id.startsWith("IDL:")) {
         // OMG IDL format

         // Check for base types
         if ("IDL:omg.org/CORBA/Object:1.0".equals(id) ||
             "IDL:omg.org/CORBA/ValueBase:1.0".equals(id))
            return null;

         // Get 2nd component of ID
         int idx2 = id.indexOf(':', 4); // 2nd colon
         if (idx2 == -1)
           return null; // invalid ID, version part missing
         String base = id.substring(4, id.indexOf(':', 4));

         // Check special prefixes
         if (base.startsWith("omg.org"))
           base = "org/omg" + base.substring(7);
         if (base.startsWith("w3c.org"))
           base = "org/w3c" + base.substring(7);

         // convert '/' to "::"
         StringBuffer b = new StringBuffer();
         for (int i = 0; i < base.length(); ++i) {
            char c = base.charAt(i);

            if (c != '/')
               b.append(c);
            else
               b.append("::");
         }

         return b.toString();
      } else if (id.startsWith("RMI:")) {
         // RMI hashed format

         // Get 2nd component of ID
         int idx2 = id.indexOf(':', 4); // 2nd colon
         if (idx2 == -1)
           return null; // invalid ID, version part missing
         String base = id.substring(4, id.indexOf(':', 4));

         // convert '.' to "::"
         StringBuffer b = new StringBuffer();
         for (int i = 0; i < base.length(); ++i) {
            char c = base.charAt(i);

            if (c != '.')
               b.append(c);
            else
               b.append("::");
         }

         return b.toString();
      } else
         return null;
   }


   // Inner classes -------------------------------------------------
}
