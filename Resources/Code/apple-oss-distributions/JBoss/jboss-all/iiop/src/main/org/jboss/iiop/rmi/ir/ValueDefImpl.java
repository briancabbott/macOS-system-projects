/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.iiop.rmi.ir;

import org.omg.CORBA.ValueDef;
import org.omg.CORBA.ValueDefOperations;
import org.omg.CORBA.ValueDefPOATie;
import org.omg.CORBA.ValueDefHelper;
import org.omg.CORBA.ValueDescription;
import org.omg.CORBA.ValueDescriptionHelper;
import org.omg.CORBA.ValueMemberDef;
import org.omg.CORBA.ValueMember;
import org.omg.CORBA.ValueMemberHelper;
import org.omg.CORBA.Any;
import org.omg.CORBA.TypeCode;
import org.omg.CORBA.TypeCodePackage.BadKind;
import org.omg.CORBA.TCKind;
import org.omg.CORBA.IRObject;
import org.omg.CORBA.Contained;
import org.omg.CORBA.ContainedPackage.Description;
import org.omg.CORBA.Container;
import org.omg.CORBA.DefinitionKind;
import org.omg.CORBA.IDLType;
import org.omg.CORBA.StructMember;
import org.omg.CORBA.UnionMember;
import org.omg.CORBA.ValueDef;
import org.omg.CORBA.ConstantDef;
import org.omg.CORBA.EnumDef;
import org.omg.CORBA.ValueBoxDef;
import org.omg.CORBA.InterfaceDef;
import org.omg.CORBA.InterfaceDefHelper;
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
import org.omg.CORBA.VM_NONE;
import org.omg.CORBA.VM_CUSTOM;
import org.omg.CORBA.VM_ABSTRACT;

import org.omg.PortableServer.POA;
import org.omg.CORBA.ValueDefPackage.FullValueDescription;

/**
 *  Interface IR object.
 *
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.4.2.1 $
 */
class ValueDefImpl
   extends ContainedImpl
   implements ValueDefOperations, LocalContainer, LocalContainedIDLType
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   private static final org.jboss.logging.Logger logger = 
               org.jboss.logging.Logger.getLogger(ValueDefImpl.class);

   // Constructors --------------------------------------------------

   ValueDefImpl(String id, String name, String version,
                    LocalContainer defined_in,
                    boolean is_abstract, boolean is_custom,
                    String[] supported_interfaces,
                    String[] abstract_base_valuetypes,
                    TypeCode baseValueTypeCode,
                    RepositoryImpl repository)
   {
      super(id, name, version, defined_in,
            DefinitionKind.dk_Value, repository);

      this.is_abstract = is_abstract;
      this.is_custom = is_custom;
      this.supported_interfaces = supported_interfaces;
      this.abstract_base_valuetypes = abstract_base_valuetypes;
      this.baseValueTypeCode = baseValueTypeCode;
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
         ref = org.omg.CORBA.ValueDefHelper.narrow(
                                servantToReference(new ValueDefPOATie(this)) );
      }
      return ref;
   }

   public void allDone()
      throws IRConstructionException
   {
      getReference();
      delegate.allDone();

      logger.debug("ValueDefImpl.allDone(): baseValueTypeCode is " +
                   ((baseValueTypeCode==null) ? "null" : "NOT null") );
      if (baseValueTypeCode != null)
         logger.debug("ValueDefImpl.allDone(): " +
                      "baseValueTypeCode.kind().value()=" +
                      baseValueTypeCode.kind().value() );
      if (baseValueTypeCode != null &&
          baseValueTypeCode.kind() != TCKind.tk_null) {
         try {
            baseValue = baseValueTypeCode.id();
            logger.debug("ValueDefImpl.allDone(): baseValue=\""
                         + baseValue + "\".");
         } catch (BadKind ex) {
            throw new IRConstructionException(
                                    "Bad kind for super-valuetype of " + id());
         }
         Contained c = repository.lookup_id(baseValue);
         logger.debug("ValueDefImpl.allDone(): c is " +
                      ((c==null) ? "null" : "NOT null") );
         base_value_ref = ValueDefHelper.narrow(c);
         logger.debug("ValueDefImpl.allDone(): base_value_ref is " +
                      ((base_value_ref==null) ? "null" : "NOT null") );
         //base_value_ref = ValueDefHelper.narrow(repository.lookup_id(baseValue));
      } else
         baseValue = "IDL:omg.org/CORBA/ValueBase:1.0"; // TODO: is this right?

      // Resolve supported interfaces
      supported_interfaces_ref = new InterfaceDef[supported_interfaces.length];
      for (int i = 0; i < supported_interfaces.length; ++i) {
         InterfaceDef iDef = InterfaceDefHelper.narrow(
                                repository.lookup_id(supported_interfaces[i]));
         if (iDef == null)
            throw new IRConstructionException(
                         "ValueDef \"" + id() + "\" unable to resolve " +
                         "reference to implemented interface \"" +
                         supported_interfaces[i] + "\".");
         supported_interfaces_ref[i] = iDef;
      }

      // Resolve abstract base valuetypes
      abstract_base_valuetypes_ref = 
         new ValueDef[abstract_base_valuetypes.length];
      for (int i = 0; i < abstract_base_valuetypes.length; ++i) {
         ValueDef vDef = ValueDefHelper.narrow(
                            repository.lookup_id(abstract_base_valuetypes[i]));
         if (vDef == null)
            throw new IRConstructionException(
                         "ValueDef \"" + id() + "\" unable to resolve " +
                         "reference to abstract base valuetype \"" +
                         abstract_base_valuetypes[i] + "\".");
         abstract_base_valuetypes_ref[i] = vDef;
      }
   }

   public void shutdown()
   {
      delegate.shutdown();
      super.shutdown();
   }


   // ContainerOperations implementation ----------------------------

   public Contained lookup(String search_name)
   {
      logger.debug("ValueDefImpl.lookup(\"" + search_name + "\") entered.");
      Contained res = delegate.lookup(search_name);
      logger.debug("ValueDefImpl.lookup(\"" + search_name +
                   "\") returning " + ((res == null) ? "null" : "non-null") );
      return res;
      //return delegate.lookup(search_name);
   }
   
   public Contained[] contents(DefinitionKind limit_type,
                               boolean exclude_inherited)
   {
      logger.debug("ValueDefImpl.contents() entered.");
      Contained[] res = delegate.contents(limit_type, exclude_inherited);
      logger.debug("ValueDefImpl.contents() " + res.length +
                   " contained to return.");
      for (int i = 0; i < res.length; ++i)
         logger.debug("  ValueDefImpl.contents() [" + i + "]: " + res[i].id());
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


   // ValueDefOperations implementation -------------------------

   public InterfaceDef[] supported_interfaces()
   {
      return supported_interfaces_ref;
   }

   public void supported_interfaces(InterfaceDef[] arg)
   {
      throw new BAD_INV_ORDER("Cannot change RMI/IIOP mapping.");
   }

   public Initializer[] initializers()
   {
      // We do not (currently) map constructors, as that is optional according
      // to the specification.
      return new Initializer[0];
   }

   public void initializers(Initializer[] arg)
   {
      throw new BAD_INV_ORDER("Cannot change RMI/IIOP mapping.");
   }

   public ValueDef base_value()
   {
      logger.debug("ValueDefImpl[" + id + "].base_value() entered.");
      if (base_value_ref == null)
         logger.debug("ValueDefImpl[" + id + "].base_value() returning NULL.");
      else
         logger.debug("ValueDefImpl[" + id + "].base_value() returning \"" +
                      base_value_ref.id() + "\".");
      return base_value_ref;
   }

   public void base_value(ValueDef arg)
   {
      throw new BAD_INV_ORDER("Cannot change RMI/IIOP mapping.");
   }

   public ValueDef[] abstract_base_values()
   {
      return abstract_base_valuetypes_ref;
   }

   public void abstract_base_values(ValueDef[] arg)
   {
      throw new BAD_INV_ORDER("Cannot change RMI/IIOP mapping.");
   }

   public boolean is_abstract()
   {
      return is_abstract;
   }

   public void is_abstract(boolean arg)
   {
      throw new BAD_INV_ORDER("Cannot change RMI/IIOP mapping.");
   }

   public boolean is_custom()
   {
      return is_custom;
   }

   public void is_custom(boolean arg)
   {
      throw new BAD_INV_ORDER("Cannot change RMI/IIOP mapping.");
   }

   public boolean is_truncatable()
   {
      return false;
   }

   public void is_truncatable(boolean arg)
   {
      throw new BAD_INV_ORDER("Cannot change RMI/IIOP mapping.");
   }

   public boolean is_a(String id)
   {
      // TODO
      return id().equals(id);
   }

   public FullValueDescription describe_value()
   {
      if (fullValueDescription != null)
         return fullValueDescription;

      // Has to create the FullValueDescription

      // TODO
      OperationDescription[] operations = new OperationDescription[0];
      AttributeDescription[] attributes = new AttributeDescription[0];

      String defined_in_id = "IDL:Global:1.0";
      if (defined_in instanceof org.omg.CORBA.ContainedOperations)
         defined_in_id = ((org.omg.CORBA.ContainedOperations)defined_in).id();

      fullValueDescription = new FullValueDescription(name, id,
                                                 is_abstract, is_custom,
                                                 defined_in_id, version,
                                                 operations, attributes,
                                                 getValueMembers(),
                                                 new Initializer[0], // TODO
                                                 supported_interfaces,
                                                 abstract_base_valuetypes,
                                                 false,
                                                 baseValue,
                                                 typeCode);

      return fullValueDescription;
   }

   public ValueMemberDef create_value_member(String id, String name,
                                             String version, IDLType type,
                                             short access)
   {
      throw new BAD_INV_ORDER("Cannot change RMI/IIOP mapping.");
   }

   public AttributeDef create_attribute(String id, String name, String version,
                                        IDLType type, AttributeMode mode)
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
      logger.debug("ValueDefImpl.type() entered.");
      if (typeCode == null) {
         short modifier = VM_NONE.value;
         if (is_custom)
            modifier = VM_CUSTOM.value;
         else if (is_abstract)
            modifier = VM_ABSTRACT.value;

         typeCode = getORB().create_value_tc(id, name, modifier,
                                             baseValueTypeCode,
                                             getValueMembersForTypeCode());
      }
      logger.debug("ValueDefImpl.type() returning.");
      return typeCode;
   }


   // ContainedImpl implementation ----------------------------------

   public Description describe()
   {
      String defined_in_id = "IR";
 
      if (defined_in instanceof org.omg.CORBA.ContainedOperations)
         defined_in_id = ((org.omg.CORBA.ContainedOperations)defined_in).id();
 
      ValueDescription md = new ValueDescription(name, id, is_abstract,
                                                 is_custom,
                                                 defined_in_id, version,
                                                 supported_interfaces,
                                                 abstract_base_valuetypes,
                                                 false, 
                                                 baseValue);
 
      Any any = getORB().create_any();

      ValueDescriptionHelper.insert(any, md);

      return new Description(DefinitionKind.dk_Value, any);
   }

   // Y overrides ---------------------------------------------------

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   /**
    *  My delegate for Container functionality.
    */
   private ContainerImplDelegate delegate;

   /**
    *  My CORBA reference.
    */
   private ValueDef ref = null;

   /**
    *  Flag that I am abstract.
    */
   private boolean is_abstract;

   /**
    *  Flag that I use custom marshaling.
    */
   private boolean is_custom;

   /**
    *  IDs of my implemented interfaces.
    */
   private String[] supported_interfaces;

   /**
    *  CORBA references to my implemented interfaces.
    */
   private InterfaceDef[] supported_interfaces_ref;

   /**
    *  IR ID of my base value (the class I extend from).
    */
   private String baseValue;

   /**
    *  TypeCode of my base value (the class I extend from).
    */
   private TypeCode baseValueTypeCode;

   /**
    *  CORBA reference to my base type.
    */
   private ValueDef base_value_ref;

   /**
    *  IDs of my abstract base valuetypes.
    */
   private String[] abstract_base_valuetypes;

   /**
    *  CORBA references to my abstract base valuetypes.
    */
   private ValueDef[] abstract_base_valuetypes_ref;

   /**
    *  My cached TypeCode.
    */
   private TypeCode typeCode;

   /**
    *  My Cached ValueMember[].
    */
   private ValueMember[] valueMembers;

   /**
    *  My cached FullValueDescription.
    */
   private FullValueDescription fullValueDescription;


   /**
    *  Create the valueMembers array, and return it.
    */
   private ValueMember[] getValueMembers()
   {
      if (valueMembers != null)
         return valueMembers;

      LocalContained[] c = _contents(DefinitionKind.dk_ValueMember, false);
      valueMembers = new ValueMember[c.length];
      for (int i = 0; i < c.length; ++i) {
         ValueMemberDefImpl vmdi = (ValueMemberDefImpl)c[i];

         valueMembers[i] = new ValueMember(vmdi.name(), vmdi.id(),
                                         ((LocalContained)vmdi.defined_in).id(),
                                           vmdi.version(),
                                           vmdi.type(), vmdi.type_def(),
                                           vmdi.access());
      }

      return valueMembers;
   }

   /**
    *  Create a valueMembers array for TypeCode creation only, and return it.
    */
   private ValueMember[] getValueMembersForTypeCode()
   {
      LocalContained[] c = _contents(DefinitionKind.dk_ValueMember, false);
      ValueMember[] vms = new ValueMember[c.length];
      for (int i = 0; i < c.length; ++i) {
         ValueMemberDefImpl vmdi = (ValueMemberDefImpl)c[i];

         vms[i] = new ValueMember(vmdi.name(),
                                  null, // ignore id
                                  null, // ignore defined_in
                                  null, // ignore version
                                  vmdi.type(),
                                  null, // ignore type_def
                                  vmdi.access());
      }

      return vms;
   }

   // Inner classes -------------------------------------------------
}
