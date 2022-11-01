------------------------------------------------------------------------------
--                                                                          --
--                         GNAT LIBRARY COMPONENTS                          --
--                                                                          --
--                        ADA.CONTAINERS.HASHED_MAPS                        --
--                                                                          --
--                                 B o d y                                  --
--                                                                          --
--             Copyright (C) 2004 Free Software Foundation, Inc.            --
--                                                                          --
-- This specification is derived from the Ada Reference Manual for use with --
-- GNAT. The copyright notice above, and the license provisions that follow --
-- apply solely to the  contents of the part following the private keyword. --
--                                                                          --
-- GNAT is free software;  you can  redistribute it  and/or modify it under --
-- terms of the  GNU General Public License as published  by the Free Soft- --
-- ware  Foundation;  either version 2,  or (at your option) any later ver- --
-- sion.  GNAT is distributed in the hope that it will be useful, but WITH- --
-- OUT ANY WARRANTY;  without even the  implied warranty of MERCHANTABILITY --
-- or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License --
-- for  more details.  You should have  received  a copy of the GNU General --
-- Public License  distributed with GNAT;  see file COPYING.  If not, write --
-- to  the Free Software Foundation,  59 Temple Place - Suite 330,  Boston, --
-- MA 02111-1307, USA.                                                      --
--                                                                          --
-- As a special exception,  if other files  instantiate  generics from this --
-- unit, or you link  this unit with other files  to produce an executable, --
-- this  unit  does not  by itself cause  the resulting  executable  to  be --
-- covered  by the  GNU  General  Public  License.  This exception does not --
-- however invalidate  any other reasons why  the executable file  might be --
-- covered by the  GNU Public License.                                      --
--                                                                          --
-- This unit was originally developed by Matthew J Heaney.                  --
------------------------------------------------------------------------------

with Ada.Unchecked_Deallocation;

with Ada.Containers.Hash_Tables.Generic_Operations;
pragma Elaborate_All (Ada.Containers.Hash_Tables.Generic_Operations);

with Ada.Containers.Hash_Tables.Generic_Keys;
pragma Elaborate_All (Ada.Containers.Hash_Tables.Generic_Keys);

package body Ada.Containers.Hashed_Maps is

   type Node_Type is limited record
      Key     : Key_Type;
      Element : Element_Type;
      Next    : Node_Access;
   end record;

   -----------------------
   -- Local Subprograms --
   -----------------------

   function Copy_Node
     (Source : Node_Access) return Node_Access;
   pragma Inline (Copy_Node);

   function Equivalent_Keys
     (Key  : Key_Type;
      Node : Node_Access) return Boolean;
   pragma Inline (Equivalent_Keys);

   function Find_Equal_Key
     (R_Map  : Map;
      L_Node : Node_Access) return Boolean;

   function Hash_Node (Node : Node_Access) return Hash_Type;
   pragma Inline (Hash_Node);

   function Next (Node : Node_Access) return Node_Access;
   pragma Inline (Next);

   function Read_Node
     (Stream : access Root_Stream_Type'Class) return Node_Access;
   pragma Inline (Read_Node);

   procedure Set_Next (Node : Node_Access; Next : Node_Access);
   pragma Inline (Set_Next);

   procedure Write_Node
     (Stream : access Root_Stream_Type'Class;
      Node   : Node_Access);
   pragma Inline (Write_Node);

   --------------------------
   -- Local Instantiations --
   --------------------------

   procedure Free is
     new Ada.Unchecked_Deallocation (Node_Type, Node_Access);

   package HT_Ops is
      new Hash_Tables.Generic_Operations
       (HT_Types          => HT_Types,
        Hash_Table_Type   => Map,
        Null_Node         => null,
        Hash_Node         => Hash_Node,
        Next              => Next,
        Set_Next          => Set_Next,
        Copy_Node         => Copy_Node,
        Free              => Free);

   package Key_Ops is
      new Hash_Tables.Generic_Keys
       (HT_Types  => HT_Types,
        HT_Type   => Map,
        Null_Node => null,
        Next      => Next,
        Set_Next  => Set_Next,
        Key_Type  => Key_Type,
        Hash      => Hash,
        Equivalent_Keys => Equivalent_Keys);

   function Is_Equal is new HT_Ops.Generic_Equal (Find_Equal_Key);

   procedure Read_Nodes  is new HT_Ops.Generic_Read (Read_Node);
   procedure Write_Nodes is new HT_Ops.Generic_Write (Write_Node);

   ---------
   -- "=" --
   ---------

   function "=" (Left, Right : Map) return Boolean renames Is_Equal;

   ------------
   -- Adjust --
   ------------

   procedure Adjust (Container : in out Map) renames HT_Ops.Adjust;

   --------------
   -- Capacity --
   --------------

   function Capacity (Container : Map) return Count_Type
     renames HT_Ops.Capacity;

   -----------
   -- Clear --
   -----------

   procedure Clear (Container : in out Map) renames HT_Ops.Clear;

   --------------
   -- Contains --
   --------------

   function Contains (Container : Map; Key : Key_Type) return Boolean is
   begin
      return Find (Container, Key) /= No_Element;
   end Contains;

   ---------------
   -- Copy_Node --
   ---------------

   function Copy_Node
     (Source : Node_Access) return Node_Access
   is
      Target : constant Node_Access :=
                 new Node_Type'(Key     => Source.Key,
                                Element => Source.Element,
                                Next    => null);
   begin
      return Target;
   end Copy_Node;

   ------------
   -- Delete --
   ------------

   procedure Delete (Container : in out Map; Key : Key_Type) is
      X : Node_Access;

   begin
      Key_Ops.Delete_Key_Sans_Free (Container, Key, X);

      if X = null then
         raise Constraint_Error;
      end if;

      Free (X);
   end Delete;

   procedure Delete (Container : in out Map; Position : in out Cursor) is
   begin
      if Position = No_Element then
         return;
      end if;

      if Position.Container /= Map_Access'(Container'Unchecked_Access) then
         raise Program_Error;
      end if;

      HT_Ops.Delete_Node_Sans_Free (Container, Position.Node);
      Free (Position.Node);

      Position.Container := null;
   end Delete;

   -------------
   -- Element --
   -------------

   function Element (Container : Map; Key : Key_Type) return Element_Type is
      C : constant Cursor := Find (Container, Key);
   begin
      return C.Node.Element;
   end Element;

   function Element (Position : Cursor) return Element_Type is
   begin
      return Position.Node.Element;
   end Element;

   ---------------------
   -- Equivalent_Keys --
   ---------------------

   function Equivalent_Keys
     (Key  : Key_Type;
      Node : Node_Access) return Boolean is
   begin
      return Equivalent_Keys (Key, Node.Key);
   end Equivalent_Keys;

   ---------------------
   -- Equivalent_Keys --
   ---------------------

   function Equivalent_Keys (Left, Right : Cursor)
     return Boolean is
   begin
      return Equivalent_Keys (Left.Node.Key, Right.Node.Key);
   end Equivalent_Keys;

   function Equivalent_Keys (Left : Cursor; Right : Key_Type) return Boolean is
   begin
      return Equivalent_Keys (Left.Node.Key, Right);
   end Equivalent_Keys;

   function Equivalent_Keys (Left : Key_Type; Right : Cursor) return Boolean is
   begin
      return Equivalent_Keys (Left, Right.Node.Key);
   end Equivalent_Keys;

   -------------
   -- Exclude --
   -------------

   procedure Exclude (Container : in out Map; Key : Key_Type) is
      X : Node_Access;
   begin
      Key_Ops.Delete_Key_Sans_Free (Container, Key, X);
      Free (X);
   end Exclude;

   --------------
   -- Finalize --
   --------------

   procedure Finalize (Container : in out Map) renames HT_Ops.Finalize;

   ----------
   -- Find --
   ----------

   function Find (Container : Map; Key : Key_Type) return Cursor is
      Node : constant Node_Access := Key_Ops.Find (Container, Key);

   begin
      if Node = null then
         return No_Element;
      end if;

      return Cursor'(Container'Unchecked_Access, Node);
   end Find;

   --------------------
   -- Find_Equal_Key --
   --------------------

   function Find_Equal_Key
     (R_Map  : Map;
      L_Node : Node_Access) return Boolean
   is
      R_Index : constant Hash_Type := Key_Ops.Index (R_Map, L_Node.Key);
      R_Node  : Node_Access := R_Map.Buckets (R_Index);

   begin
      while R_Node /= null loop
         if Equivalent_Keys (L_Node.Key, R_Node.Key) then
            return L_Node.Element = R_Node.Element;
         end if;

         R_Node := R_Node.Next;
      end loop;

      return False;
   end Find_Equal_Key;

   -----------
   -- First --
   -----------

   function First (Container : Map) return Cursor is
      Node : constant Node_Access := HT_Ops.First (Container);

   begin
      if Node = null then
         return No_Element;
      end if;

      return Cursor'(Container'Unchecked_Access, Node);
   end First;

   -----------------
   -- Has_Element --
   -----------------

   function Has_Element (Position : Cursor) return Boolean is
   begin
      return Position /= No_Element;
   end Has_Element;

   ---------------
   -- Hash_Node --
   ---------------

   function Hash_Node (Node : Node_Access) return Hash_Type is
   begin
      return Hash (Node.Key);
   end Hash_Node;

   -------------
   -- Include --
   -------------

   procedure Include
     (Container : in out Map;
      Key       : Key_Type;
      New_Item  : Element_Type)
   is
      Position : Cursor;
      Inserted : Boolean;

   begin
      Insert (Container, Key, New_Item, Position, Inserted);

      if not Inserted then
         Position.Node.Key := Key;
         Position.Node.Element := New_Item;
      end if;
   end Include;

   ------------
   -- Insert --
   ------------

   procedure Insert
     (Container : in out Map;
      Key       : Key_Type;
      Position  : out Cursor;
      Inserted  : out Boolean)
   is
      function New_Node (Next : Node_Access) return Node_Access;
      pragma Inline (New_Node);

      procedure Local_Insert is
        new Key_Ops.Generic_Conditional_Insert (New_Node);

      --------------
      -- New_Node --
      --------------

      function New_Node (Next : Node_Access) return Node_Access is
         Node : Node_Access := new Node_Type; --  Ada 2005 aggregate possible?

      begin
         Node.Key := Key;
         Node.Next := Next;

         return Node;

      exception
         when others =>
            Free (Node);
            raise;
      end New_Node;

   --  Start of processing for Insert

   begin
      HT_Ops.Ensure_Capacity (Container, Container.Length + 1);
      Local_Insert (Container, Key, Position.Node, Inserted);
      Position.Container := Container'Unchecked_Access;
   end Insert;

   procedure Insert
     (Container : in out Map;
      Key       : Key_Type;
      New_Item  : Element_Type;
      Position  : out Cursor;
      Inserted  : out Boolean)
   is
      function New_Node (Next : Node_Access) return Node_Access;
      pragma Inline (New_Node);

      procedure Local_Insert is
        new Key_Ops.Generic_Conditional_Insert (New_Node);

      --------------
      -- New_Node --
      --------------

      function New_Node (Next : Node_Access) return Node_Access is
         Node : constant Node_Access := new Node_Type'(Key, New_Item, Next);
      begin
         return Node;
      end New_Node;

   --  Start of processing for Insert

   begin
      HT_Ops.Ensure_Capacity (Container, Container.Length + 1);
      Local_Insert (Container, Key, Position.Node, Inserted);
      Position.Container := Container'Unchecked_Access;
   end Insert;

   procedure Insert
     (Container : in out Map;
      Key       : Key_Type;
      New_Item  : Element_Type)
   is
      Position : Cursor;
      Inserted : Boolean;

   begin
      Insert (Container, Key, New_Item, Position, Inserted);

      if not Inserted then
         raise Constraint_Error;
      end if;
   end Insert;

   --------------
   -- Is_Empty --
   --------------

   function Is_Empty (Container : Map) return Boolean is
   begin
      return Container.Length = 0;
   end Is_Empty;

   -------------
   -- Iterate --
   -------------

   procedure Iterate
     (Container : Map;
      Process   : not null access procedure (Position : Cursor))
   is
      procedure Process_Node (Node : Node_Access);
      pragma Inline (Process_Node);

      procedure Local_Iterate is new HT_Ops.Generic_Iteration (Process_Node);

      ------------------
      -- Process_Node --
      ------------------

      procedure Process_Node (Node : Node_Access) is
      begin
         Process (Cursor'(Container'Unchecked_Access, Node));
      end Process_Node;

   --  Start of processing for Iterate

   begin
      Local_Iterate (Container);
   end Iterate;

   ---------
   -- Key --
   ---------

   function Key (Position : Cursor) return Key_Type is
   begin
      return Position.Node.Key;
   end Key;

   ------------
   -- Length --
   ------------

   function Length (Container : Map) return Count_Type is
   begin
      return Container.Length;
   end Length;

   ----------
   -- Move --
   ----------

   procedure Move
     (Target : in out Map;
      Source : in out Map) renames HT_Ops.Move;

   ----------
   -- Next --
   ----------

   function Next (Node : Node_Access) return Node_Access is
   begin
      return Node.Next;
   end Next;

   function Next (Position : Cursor) return Cursor is
   begin
      if Position = No_Element then
         return No_Element;
      end if;

      declare
         M    : Map renames Position.Container.all;
         Node : constant Node_Access := HT_Ops.Next (M, Position.Node);

      begin
         if Node = null then
            return No_Element;
         end if;

         return Cursor'(Position.Container, Node);
      end;
   end Next;

   procedure Next (Position : in out Cursor) is
   begin
      Position := Next (Position);
   end Next;

   -------------------
   -- Query_Element --
   -------------------

   procedure Query_Element
     (Position : Cursor;
      Process  : not null access procedure (Element : Element_Type))
   is
   begin
      Process (Position.Node.Key, Position.Node.Element);
   end Query_Element;

   ----------
   -- Read --
   ----------

   procedure Read
     (Stream    : access Root_Stream_Type'Class;
      Container : out Map) renames Read_Nodes;

   ---------------
   -- Read_Node --
   ---------------

   function Read_Node
     (Stream : access Root_Stream_Type'Class) return Node_Access
   is
      Node : Node_Access := new Node_Type;

   begin
      Key_Type'Read (Stream, Node.Key);
      Element_Type'Read (Stream, Node.Element);
      return Node;

   exception
      when others =>
         Free (Node);
         raise;
   end Read_Node;

   -------------
   -- Replace --
   -------------

   procedure Replace
     (Container : in out Map;
      Key       : Key_Type;
      New_Item  : Element_Type)
   is
      Node : constant Node_Access := Key_Ops.Find (Container, Key);

   begin
      if Node = null then
         raise Constraint_Error;
      end if;

      Node.Key := Key;
      Node.Element := New_Item;
   end Replace;

   ---------------------
   -- Replace_Element --
   ---------------------

   procedure Replace_Element (Position : Cursor; By : Element_Type) is
   begin
      Position.Node.Element := By;
   end Replace_Element;

   ----------------------
   -- Reserve_Capacity --
   ----------------------

   procedure Reserve_Capacity
     (Container : in out Map;
      Capacity  : Count_Type) renames HT_Ops.Ensure_Capacity;

   --------------
   -- Set_Next --
   --------------

   procedure Set_Next (Node : Node_Access; Next : Node_Access) is
   begin
      Node.Next := Next;
   end Set_Next;

   --------------------
   -- Update_Element --
   --------------------

   procedure Update_Element
     (Position : Cursor;
      Process  : not null access procedure (Element : in out Element_Type))
   is
   begin
      Process (Position.Node.Key, Position.Node.Element);
   end Update_Element;

   -----------
   -- Write --
   -----------

   procedure Write
     (Stream    : access Root_Stream_Type'Class;
      Container : Map) renames Write_Nodes;

   ----------------
   -- Write_Node --
   ----------------

   procedure Write_Node
     (Stream : access Root_Stream_Type'Class;
      Node   : Node_Access)
   is
   begin
      Key_Type'Write (Stream, Node.Key);
      Element_Type'Write (Stream, Node.Element);
   end Write_Node;

end Ada.Containers.Hashed_Maps;
