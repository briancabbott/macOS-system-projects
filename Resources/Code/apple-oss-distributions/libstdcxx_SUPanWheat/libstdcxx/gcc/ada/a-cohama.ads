------------------------------------------------------------------------------
--                                                                          --
--                         GNAT LIBRARY COMPONENTS                          --
--                                                                          --
--                        ADA.CONTAINERS.HASHED_MAPS                        --
--                                                                          --
--                                 S p e c                                  --
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

with Ada.Containers.Hash_Tables;
with Ada.Streams;

generic
   type Key_Type is private;

   type Element_Type is private;

   with function Hash (Key : Key_Type) return Hash_Type;

   with function Equivalent_Keys (Left, Right : Key_Type) return Boolean;

   with function "=" (Left, Right : Element_Type) return Boolean is <>;

package Ada.Containers.Hashed_Maps is
pragma Preelaborate (Hashed_Maps);

   type Map is tagged private;

   type Cursor is private;

   Empty_Map : constant Map;

   No_Element : constant Cursor;

   function "=" (Left, Right : Map) return Boolean;

   function Length (Container : Map) return Count_Type;

   function Is_Empty (Container : Map) return Boolean;

   procedure Clear (Container : in out Map);

   function Element (Position : Cursor)
      return Element_Type;

   procedure Query_Element
     (Position : Cursor;
      Process  : not null access
                   procedure (Key : Key_Type; Element : Element_Type));

   procedure Update_Element
     (Position : Cursor;
      Process  : not null access
                   procedure (Key : Key_Type; Element : in out Element_Type));

   procedure Replace_Element (Position : Cursor; By : Element_Type);

   procedure Move (Target : in out Map; Source : in out Map);

   procedure Insert
     (Container : in out Map;
      Key       : Key_Type;
      New_Item  : Element_Type;
      Position  : out Cursor;
      Inserted  : out Boolean);

   procedure Insert
     (Container : in out Map;
      Key       : Key_Type;
      New_Item  : Element_Type);

   procedure Include
     (Container : in out Map;
      Key       : Key_Type;
      New_Item  : Element_Type);

   procedure Replace
     (Container : in out Map;
      Key       : Key_Type;
      New_Item  : Element_Type);

   procedure Insert
     (Container : in out Map;
      Key       : Key_Type;
      Position  : out Cursor;
      Inserted  : out Boolean);

   procedure Delete (Container : in out Map; Key : Key_Type);

   procedure Exclude (Container : in out Map; Key : Key_Type);

   procedure Delete (Container : in out Map; Position : in out Cursor);

   function Contains (Container : Map; Key : Key_Type) return Boolean;

   function Find (Container : Map; Key : Key_Type) return Cursor;

   function Element (Container : Map; Key : Key_Type) return Element_Type;

   function Capacity (Container : Map) return Count_Type;

   procedure Reserve_Capacity (Container : in out Map;
                               Capacity  : Count_Type);

   function First (Container : Map) return Cursor;

   function Next (Position : Cursor) return Cursor;

   procedure Next (Position : in out Cursor);

   function Has_Element (Position : Cursor) return Boolean;

   function Key (Position : Cursor) return Key_Type;

   function Equivalent_Keys (Left, Right : Cursor) return Boolean;

   function Equivalent_Keys (Left : Cursor; Right : Key_Type) return Boolean;

   function Equivalent_Keys (Left : Key_Type; Right : Cursor) return Boolean;

   procedure Iterate
     (Container : Map;
      Process   : not null access procedure (Position : Cursor));

private

   type Node_Type;
   type Node_Access is access Node_Type;

   package HT_Types is new Hash_Tables.Generic_Hash_Table_Types (Node_Access);

   use HT_Types;

   type Map is new Hash_Table_Type with null record;

   procedure Adjust (Container : in out Map);

   procedure Finalize (Container : in out Map);

   use Ada.Streams;

   procedure Write
     (Stream    : access Root_Stream_Type'Class;
      Container : Map);

   for Map'Write use Write;

   procedure Read
     (Stream    : access Root_Stream_Type'Class;
      Container : out Map);

   for Map'Read use Read;

   Empty_Map : constant Map := (Hash_Table_Type with null record);

   type Map_Access is access constant Map;
   for Map_Access'Storage_Size use 0;

   type Cursor is
      record
         Container : Map_Access;
         Node      : Node_Access;
      end record;

   No_Element : constant Cursor := (Container => null, Node => null);

end Ada.Containers.Hashed_Maps;
