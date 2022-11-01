------------------------------------------------------------------------------
--                                                                          --
--                         GNAT COMPILER COMPONENTS                         --
--                                                                          --
--                                  O P T                                   --
--                                                                          --
--                                 B o d y                                  --
--                                                                          --
--          Copyright (C) 1992-2004, Free Software Foundation, Inc.         --
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
-- GNAT was originally developed  by the GNAT team at  New York University. --
-- Extensive contributions were provided by Ada Core Technologies Inc.      --
--                                                                          --
------------------------------------------------------------------------------

with Gnatvsn; use Gnatvsn;
with System;  use System;
with Tree_IO; use Tree_IO;

package body Opt is

   Immediate_Errors : Boolean := True;
   --  This is an obsolete flag that is no longer present in opt.ads. We
   --  retain it here because this flag was written to the tree and there
   --  is no point in making trees incomaptible just for the sake of saving
   --  one byte of data. The value written is ignored.

   ----------------------------------
   -- Register_Opt_Config_Switches --
   ----------------------------------

   procedure Register_Opt_Config_Switches is
   begin
      Ada_Version_Config                    := Ada_Version;
      Dynamic_Elaboration_Checks_Config     := Dynamic_Elaboration_Checks;
      Exception_Locations_Suppressed_Config := Exception_Locations_Suppressed;
      Extensions_Allowed_Config             := Extensions_Allowed;
      External_Name_Exp_Casing_Config       := External_Name_Exp_Casing;
      External_Name_Imp_Casing_Config       := External_Name_Imp_Casing;
      Polling_Required_Config               := Polling_Required;
      Use_VADS_Size_Config                  := Use_VADS_Size;
   end Register_Opt_Config_Switches;

   ---------------------------------
   -- Restore_Opt_Config_Switches --
   ---------------------------------

   procedure Restore_Opt_Config_Switches (Save : Config_Switches_Type) is
   begin
      Ada_Version                    := Save.Ada_Version;
      Dynamic_Elaboration_Checks     := Save.Dynamic_Elaboration_Checks;
      Exception_Locations_Suppressed := Save.Exception_Locations_Suppressed;
      Extensions_Allowed             := Save.Extensions_Allowed;
      External_Name_Exp_Casing       := Save.External_Name_Exp_Casing;
      External_Name_Imp_Casing       := Save.External_Name_Imp_Casing;
      Polling_Required               := Save.Polling_Required;
      Use_VADS_Size                  := Save.Use_VADS_Size;
   end Restore_Opt_Config_Switches;

   ------------------------------
   -- Save_Opt_Config_Switches --
   ------------------------------

   procedure Save_Opt_Config_Switches (Save : out Config_Switches_Type) is
   begin
      Save.Ada_Version                    := Ada_Version;
      Save.Dynamic_Elaboration_Checks     := Dynamic_Elaboration_Checks;
      Save.Exception_Locations_Suppressed := Exception_Locations_Suppressed;
      Save.Extensions_Allowed             := Extensions_Allowed;
      Save.External_Name_Exp_Casing       := External_Name_Exp_Casing;
      Save.External_Name_Imp_Casing       := External_Name_Imp_Casing;
      Save.Polling_Required               := Polling_Required;
      Save.Use_VADS_Size                  := Use_VADS_Size;
   end Save_Opt_Config_Switches;

   -----------------------------
   -- Set_Opt_Config_Switches --
   -----------------------------

   procedure Set_Opt_Config_Switches (Internal_Unit : Boolean) is
   begin
      if Internal_Unit then
         Ada_Version                := Ada_Version_Runtime;
         Dynamic_Elaboration_Checks := False;
         Extensions_Allowed         := True;
         External_Name_Exp_Casing   := As_Is;
         External_Name_Imp_Casing   := Lowercase;
         Use_VADS_Size              := False;

      else
         Ada_Version                := Ada_Version_Config;
         Dynamic_Elaboration_Checks := Dynamic_Elaboration_Checks_Config;
         Extensions_Allowed         := Extensions_Allowed_Config;
         External_Name_Exp_Casing   := External_Name_Exp_Casing_Config;
         External_Name_Imp_Casing   := External_Name_Imp_Casing_Config;
         Use_VADS_Size              := Use_VADS_Size_Config;
      end if;

      Exception_Locations_Suppressed := Exception_Locations_Suppressed_Config;
      Polling_Required               := Polling_Required_Config;
   end Set_Opt_Config_Switches;

   ---------------
   -- Tree_Read --
   ---------------

   procedure Tree_Read is
      Tree_Version_String_Len : Nat;
      Ada_Version_Config_Val  : Nat;

   begin
      Tree_Read_Int  (Tree_ASIS_Version_Number);
      Tree_Read_Bool (Brief_Output);
      Tree_Read_Bool (GNAT_Mode);
      Tree_Read_Char (Identifier_Character_Set);
      Tree_Read_Int  (Maximum_File_Name_Length);
      Tree_Read_Data (Suppress_Options'Address,
                      Suppress_Array'Object_Size / Storage_Unit);
      Tree_Read_Bool (Verbose_Mode);
      Tree_Read_Data (Warning_Mode'Address,
                      Warning_Mode_Type'Object_Size / Storage_Unit);
      Tree_Read_Int  (Ada_Version_Config_Val);
      Tree_Read_Bool (All_Errors_Mode);
      Tree_Read_Bool (Assertions_Enabled);
      Tree_Read_Bool (Enable_Overflow_Checks);
      Tree_Read_Bool (Full_List);

      Ada_Version_Config := Ada_Version_Type'Val (Ada_Version_Config_Val);

      --  Read version string: we have to check the length first

      Tree_Read_Int (Tree_Version_String_Len);

      if Tree_Version_String_Len = Tree_Version_String'Length then
         Tree_Read_Data
           (Tree_Version_String'Address, Tree_Version_String_Len);
      else
         Tree_Version_String := (others => '?');

         declare
            Tmp : String (1 .. Integer (Tree_Version_String_Len));
         begin
            Tree_Read_Data
              (Tmp'Address, Tree_Version_String_Len);
         end;

      end if;

      Tree_Read_Data (Distribution_Stub_Mode'Address,
                      Distribution_Stub_Mode_Type'Object_Size / Storage_Unit);
      Tree_Read_Bool (Immediate_Errors);
      Tree_Read_Bool (Inline_Active);
      Tree_Read_Bool (Inline_Processing_Required);
      Tree_Read_Bool (List_Units);
      Tree_Read_Bool (Configurable_Run_Time_Mode);
      Tree_Read_Data (Operating_Mode'Address,
                      Operating_Mode_Type'Object_Size / Storage_Unit);
      Tree_Read_Bool (Suppress_Checks);
      Tree_Read_Bool (Try_Semantics);
      Tree_Read_Data (Wide_Character_Encoding_Method'Address,
                      WC_Encoding_Method'Object_Size / Storage_Unit);
      Tree_Read_Bool (Upper_Half_Encoding);
      Tree_Read_Bool (Force_ALI_Tree_File);
   end Tree_Read;

   ----------------
   -- Tree_Write --
   ----------------

   procedure Tree_Write is
      Version_String : String := Gnat_Version_String;
   begin
      Tree_Write_Int  (ASIS_Version_Number);
      Tree_Write_Bool (Brief_Output);
      Tree_Write_Bool (GNAT_Mode);
      Tree_Write_Char (Identifier_Character_Set);
      Tree_Write_Int  (Maximum_File_Name_Length);
      Tree_Write_Data (Suppress_Options'Address,
                       Suppress_Array'Object_Size / Storage_Unit);
      Tree_Write_Bool (Verbose_Mode);
      Tree_Write_Data (Warning_Mode'Address,
                       Warning_Mode_Type'Object_Size / Storage_Unit);
      Tree_Write_Int  (Ada_Version_Type'Pos (Ada_Version_Config));
      Tree_Write_Bool (All_Errors_Mode);
      Tree_Write_Bool (Assertions_Enabled);
      Tree_Write_Bool (Enable_Overflow_Checks);
      Tree_Write_Bool (Full_List);
      Tree_Write_Int  (Int (Version_String'Length));
      Tree_Write_Data (Version_String'Address,
                       Version_String'Length);
      Tree_Write_Data (Distribution_Stub_Mode'Address,
                       Distribution_Stub_Mode_Type'Object_Size / Storage_Unit);
      Tree_Write_Bool (Immediate_Errors);
      Tree_Write_Bool (Inline_Active);
      Tree_Write_Bool (Inline_Processing_Required);
      Tree_Write_Bool (List_Units);
      Tree_Write_Bool (Configurable_Run_Time_Mode);
      Tree_Write_Data (Operating_Mode'Address,
                       Operating_Mode_Type'Object_Size / Storage_Unit);
      Tree_Write_Bool (Suppress_Checks);
      Tree_Write_Bool (Try_Semantics);
      Tree_Write_Data (Wide_Character_Encoding_Method'Address,
                       WC_Encoding_Method'Object_Size / Storage_Unit);
      Tree_Write_Bool (Upper_Half_Encoding);
      Tree_Write_Bool (Force_ALI_Tree_File);
   end Tree_Write;

end Opt;
