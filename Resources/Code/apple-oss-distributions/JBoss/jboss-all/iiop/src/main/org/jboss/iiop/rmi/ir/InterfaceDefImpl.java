/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.iiop.rmi.ir;

import org.omg.CORBA.InterfaceDefOperations;
import org.omg.CORBA.InterfaceDefPOATie;
import org.omg.CORBA.InterfaceDefHelper;
import org.omg.CORBA.Any;
import org.omg.CORBA.TypeCode;
import org.omg.CORBA.IRObject;
import org.omg.CORBA.Contained;
import org.omg.CORBA.ContainedPackage.Description;
import org.omg.CORBA.Container;
import org.omg.CORBA.DefinitionKind;
import org.omg.CORBA.IDLType;
import org.omg.CORBA.StructMember;
import org.omg.CORBA.UnionMember;
import org.omg.CORBA.InterfaceDef;
import org.omg.CORBA.ConstantDef;
import org.omg.CORBA.EnumDef;
import org.omg.CORBA.ValueDef;
import org.omg.CORBA.ValueBoxDef;
import org.omg.CORBA.Initializer;
import org.omg.CORBA.StructDef;
import org.omg.CORBA.UnionDef;
import org.omg.CORBA.ModuleDef;
import org.omg.CORBA.AliasDef;
import org.omg.CORBA.NativeDef;
import org.omg.CORBA.OperationDef;
import org.omg.CORBA.OperationMode;
import org.omg.CORBA.ParameterDescription;
import org.omg.CORBA.AttributeDef;
import org.omg.CORBA.AttributeMode;
import org.omg.CORBA.ExceptionDef;
import org.omg.CORBA.OperationDescription;
import org.omg.CORBA.AttributeDescription;
import org.omg.CORBA.InterfaceDescription;
import org.omg.CORBA.InterfaceDescriptionHelper;
import org.omg.CORBA.BAD_INV_ORDER;
import org.omg.PortableServer.POA;
import org.omg.CORBA.InterfaceDefPackage.FullInterfaceDescription;

/**
 *  Interface IR object.
 *
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.3 $
 */
class InterfaceDefImpl
   extends ContainedImpl
   implements InterfaceDefOperations, LocalContainer
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   private static final org.jboss.logging.Logger logger = 
               org.jboss.logging.Logger.getLogger(InterfaceDefImpl.class);

   // Constructors --------------------------------------------------

   InterfaceDefImpl(String id, String name, String version,
                    LocalContainer defined_in, String[] base_interfaces,
                    RepositoryImpl repository)
   {
      super(id, name, version, defined_in,
            DefinitionKind.dk_Interface, repository);

      this.base_interfaces = base_interfaces;
      this.delegate = new ContainerImplDelegate(this);
   }

   // Public --------------------------------------------------------

   // LocalContainer implementation ---------------------------------

   public LocalContained _lookup(String search_name)
   {
      return delegate._lookup(search_name);
   }
 
   public LocalContained[] _contents(DefinitionKind limit_type,
                                    boolean exclude_inherited)
   {
      return delegate._contents(limit_type, exclude_inherited);
   }
 
   public LocalContained[] _lookup_name(String search_name,
                                         int levels_to_search,
                                         DefinitionKind limit_type,
                                         boolean exclude_inherited)
   {
      return delegate._lookup_name(search_name, levels_to_search, limit_type,
                                   exclude_inherited);
   }

   public void add(String name, LocalContained contained)
      throws IRConstructionException
   {
      delegate.add(name, contained);
   }

   // LocalIRObject implementation ---------------------------------

   public IRObject getReference()
   {
      if (ref == null) {
         ref = org.omg.CORBA.InterfaceDefHelper.narrow(
                            servantToReference(new InterfaceDefPOATie(this)) );
      }
      return ref;
   }

   public void allDone()
      throws IRConstructionException
   {
      getReference();
      delegate.allDone();
   }

   public void shutdown()
   {
      delegate.shutdown();
      super.shutdown();
   }

   // ContainerOperations implementation ----------------------------

   public Contained lookup(String search_name)
   {
      logger.debug("InterfaceDefImpl.lookup(\"" + search_name + 
                   "\") entered.");
      Contained res = delegate.lookup(search_name);
      logger.debug("InterfaceDefImpl.lookup(\"" + search_name +
                   "\") returning " + ((res == null) ? "null" : "non-null") );
      return res;
      //return delegate.lookup(search_name);
   }

   public Contained[] contents(DefinitionKind limit_type,
                               boolean exclude_inherited)
   {
      logger.debug("InterfaceDefImpl.contents() entered.");
      Contained[] res = delegate.contents(limit_type, exclude_inherited);
      logger.debug("InterfaceDefImpl.contents() " + res.length + 
                   " contained to return.");
      for (int i = 0; i < res.length; ++i)
         logger.debug("  InterfaceDefImpl.contents() [" + i + "]: " + 
                      res[i].id()); 
      return res;
      //return delegate.contents(limit_type, exclude_inherited);
   }

   public Contained[] lookup_name(String search_name, int levels_to_search,
                                  DefinitionKind limit_type,
                                  boolean exclude_inherited)
   {
      return delegate.lookup_name(search_name, levels_to_search, limit_type,
                                  exclude_inherited);
   }

   public org.omg.CORBA.ContainerPackage.Description[]
                        describe_contents(DefinitionKind limit_type,
                                          boolean exclude_inherited,
                                          int max_returned_objs)
   {
      return delegate.describe_contents(limit_type, exclude_inherited,
                                        max_returned_objs);
   }

   public ModuleDef create_module(String id, String name, String version)
   {
      return delegate.create_module(id, name, version);
   }

   public ConstantDef create_constant(String id, String name, String version,
                                      IDLType type, Any value)
   {
      return delegate.create_constant(id, name, version, type, value);
   }

   public StructDef create_struct(String id, String name, String version,
                                  StructMember[] members)
   {
      return delegate.create_struct(id, name, version, members);
   }

   public UnionDef create_union(String id, String name, String version,
                                IDLType discriminator_type,
                                UnionMember[] members)
   {
      return delegate.create_union(id, name, version, discriminator_type,
                                   members);
   }

   public EnumDef create_enum(String id, String name, String version,
                              String[] members)
   {
      return delegate.create_enum(id, name, version, members);
   }

   public AliasDef create_alias(String id, String name, String version,
                                IDLType original_type)
   {
      return delegate.create_alias(id, name, version, original_type);
   }

   public InterfaceDef create_interface(String id, String name, String version,
                                        InterfaceDef[] base_interfaces,
                                        boolean is_abstract)
   {
      return delegate.create_interface(id, name, version,
                                       base_interfaces, is_abstract);
   }

   public ValueDef create_value(String id, String name, String version,
                                boolean is_custom, boolean is_abstract,
                                ValueDef base_value, boolean is_truncatable,
                                ValueDef[] abstract_base_values,
                                InterfaceDef[] supported_interfaces,
                                Initializer[] initializers)
   {
      return delegate.create_value(id, name, version, is_custom, is_abstract,
                                   base_value, is_truncatable,
                                   abstract_base_values, supported_interfaces,
                                   initializers);
   }

   public ValueBoxDef create_value_box(String id, String name, String version,
                                       IDLType original_type_def)
   {
      return delegate.create_value_box(id, name, version, original_type_def);
   }

   public ExceptionDef create_exception(String id, String name, String version,
                                        StructMember[] members)
   {
      return delegate.create_exception(id, name, version, members);
   }

   public NativeDef create_native(String id, String name, String version)
   {
      return delegate.create_native(id, name, version);
   }


   // InterfaceDefOperations implementation -------------------------

   public InterfaceDef[] base_interfaces()
   {
      if (base_interfaces_ref == null) {
         base_interfaces_ref = new InterfaceDef[base_interfaces.length];
         for (int i = 0; i < base_interfaces_ref.length; ++i) {
            logger.debug("InterfaceDefImpl.base_interfaces(): " +
                         "looking up \"" + base_interfaces[i] + "\".");
            Contained c = repository.lookup_id(base_interfaces[i]);
            logger.debug("InterfaceDefImpl.base_interfaces(): " +
                         "Got: " + ((c==null)? "null" : c.id()));
            base_interfaces_ref[i] = InterfaceDefHelper.narrow(c);
            logger.debug("InterfaceDefImpl.base_interfaces(): " +
                         "ref: " + ((c==null)? "null" : "not null"));
         }
      }

      return base_interfaces_ref;
   }

   public void base_interfaces(org.omg.CORBA.InterfaceDef[] arg)
   {
      throw new BAD_INV_ORDER("Cannot change RMI/IIOP mapping.");
   }

   public boolean is_abstract()
   {
      return false;
   }

   public void is_abstract(boolean arg)
   {
      throw new BAD_INV_ORDER("Cannot change RMI/IIOP mapping.");
   }

   public boolean is_a(java.lang.String interface_id)
   {
      // TODO
      return false;
   }

   public FullInterfaceDescription describe_interface()
   {
      if (fullInterfaceDescription != null)
         return fullInterfaceDescription;

      // Has to create the FullInterfaceDescription

      // TODO
      OperationDescription[] operations = new OperationDescription[0];
      AttributeDescription[] attributes = new AttributeDescription[0];

      String defined_in_id = "IDL:Global:1.0";
      if (defined_in instanceof org.omg.CORBA.ContainedOperations)
         defined_in_id = ((org.omg.CORBA.ContainedOperations)defined_in).id();

      fullInterfaceDescription = new FullInterfaceDescription(name, id,
                                                              defined_in_id,
                                                              version,
                                                              operations,
                                                              attributes,
                                                              base_interfaces,
                                                              type(),
                                                              is_abstract);

      return fullInterfaceDescription;
   }

   public AttributeDef create_attribute(String id, String name,
                                        String version, IDLType type,
                                        AttributeMode mode)
   {
      throw new BAD_INV_ORDER("Cannot change RMI/IIOP mapping.");
   }

   public OperationDef create_operation(String id, String name, String version,
                                        IDLType result, OperationMode mode,
                                        ParameterDescription[] params,
                                        ExceptionDef[] exceptions,
                                        String[] contexts)
   {
      throw new BAD_INV_ORDER("Cannot change RMI/IIOP mapping.");
   }


   // IDLTypeOperations implementation ------------------------------

   public TypeCode type()
   {
      if (typeCode == null)
         typeCode = getORB().create_interface_tc(id, name);

      return typeCode;
   }

   // ContainedImpl implementation ----------------------------------

   public Description describe()
   {
      String defined_in_id = "IR";
 
      if (defined_in instanceof org.omg.CORBA.ContainedOperations)
         defined_in_id = ((org.omg.CORBA.ContainedOperations)defined_in).id();
 
      org.omg.CORBA.InterfaceDescription md =
                   new InterfaceDescription(name, id, defined_in_id, version,
                                            base_interfaces, false);
 
      Any any = getORB().create_any();

      InterfaceDescriptionHelper.insert(any, md);

      return new Description(DefinitionKind.dk_Interface, any);
   }

   // Y overrides ---------------------------------------------------

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   /**
    *  My CORBA reference.
    */
   protected InterfaceDef ref = null;

   /**
    *  My cached TypeCode.
    */
   protected TypeCode typeCode;

   // Private -------------------------------------------------------

   /**
    *  My delegate for Container functionality.
    */
   private ContainerImplDelegate delegate;

   /**
    *  Flag that I am abstract.
    */
   private boolean is_abstract;

   /**
    *  IDs of my superinterfaces.
    */
   private String[] base_interfaces;

   /**
    *  CORBA references of my superinterfaces.
    */
   private InterfaceDef[] base_interfaces_ref;

   /**
    *  My cached FullInterfaceDescription.
    */
   private FullInterfaceDescription fullInterfaceDescription;

   // Inner classes -------------------------------------------------
}
