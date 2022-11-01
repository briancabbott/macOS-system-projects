/****************************************************************************
 *                                                                          *
 *                         GNAT COMPILER COMPONENTS                         *
 *                                                                          *
 *                               S N A M E S                                *
 *                                                                          *
 *                              C Header File                               *
 *                                                                          *
 *          Copyright (C) 1992-2005, Free Software Foundation, Inc.         *
 *                                                                          *
 * GNAT is free software;  you can  redistribute it  and/or modify it under *
 * terms of the  GNU General Public License as published  by the Free Soft- *
 * ware  Foundation;  either version 2,  or (at your option) any later ver- *
 * sion.  GNAT is distributed in the hope that it will be useful, but WITH- *
 * OUT ANY WARRANTY;  without even the  implied warranty of MERCHANTABILITY *
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License *
 * for  more details.  You should have  received  a copy of the GNU General *
 * Public License  distributed with GNAT;  see file COPYING.  If not, write *
 * to  the Free Software Foundation,  59 Temple Place - Suite 330,  Boston, *
 * MA 02111-1307, USA.                                                      *
 *                                                                          *
 * GNAT was originally developed  by the GNAT team at  New York University. *
 * Extensive contributions were provided by Ada Core Technologies Inc.      *
 *                                                                          *
 ****************************************************************************/

/* This is the C file that corresponds to the Ada package specification
   Snames. It was created manually from the file snames.ads. */

/* Name_Id values */

#define Name_uParent    (First_Name_Id + 256 + 0)
#define Name_uTag       (First_Name_Id + 256 + 1)
#define Name_Off        (First_Name_Id + 256 + 2)
#define Name_Space      (First_Name_Id + 256 + 3)
#define Name_Time       (First_Name_Id + 256 + 4)

/* Define the function to return one of the numeric values below. Note
   that it actually returns a char since an enumeration value of less
   than 256 entries is represented that way in Ada.  The operand is a Chars
   field value.  */

#define Get_Attribute_Id snames__get_attribute_id
extern unsigned char Get_Attribute_Id (int);

/* Define the numeric values for attributes.  */

#define  Attr_Abort_Signal                   0
#define  Attr_Access                         1
#define  Attr_Address                        2
#define  Attr_Address_Size                   3
#define  Attr_Aft                            4
#define  Attr_Alignment                      5
#define  Attr_Asm_Input                      6
#define  Attr_Asm_Output                     7
#define  Attr_AST_Entry                      8
#define  Attr_Bit                            9
#define  Attr_Bit_Order                     10
#define  Attr_Bit_Position                  11
#define  Attr_Body_Version                  12
#define  Attr_Callable                      13
#define  Attr_Caller                        14
#define  Attr_Code_Address                  15
#define  Attr_Component_Size                16
#define  Attr_Compose                       17
#define  Attr_Constrained                   18
#define  Attr_Count                         19
#define  Attr_Default_Bit_Order             20
#define  Attr_Definite                      21
#define  Attr_Delta                         22
#define  Attr_Denorm                        23
#define  Attr_Digits                        24
#define  Attr_Elaborated                    25
#define  Attr_Emax                          26
#define  Attr_Enum_Rep                      27
#define  Attr_Epsilon                       28
#define  Attr_Exponent                      29
#define  Attr_External_Tag                  30
#define  Attr_First                         31
#define  Attr_First_Bit                     32
#define  Attr_Fixed_Value                   33
#define  Attr_Fore                          34
#define  Attr_Has_Access_Values             35
#define  Attr_Has_Discriminants             36
#define  Attr_Identity                      37
#define  Attr_Img                           38
#define  Attr_Integer_Value                 39
#define  Attr_Large                         40
#define  Attr_Last                          41
#define  Attr_Last_Bit                      42
#define  Attr_Leading_Part                  43
#define  Attr_Length                        44
#define  Attr_Machine_Emax                  45
#define  Attr_Machine_Emin                  46
#define  Attr_Machine_Mantissa              47
#define  Attr_Machine_Overflows             48
#define  Attr_Machine_Radix                 49
#define  Attr_Machine_Rounds                50
#define  Attr_Machine_Size                  51
#define  Attr_Mantissa                      52
#define  Attr_Max_Size_In_Storage_Elements  53
#define  Attr_Maximum_Alignment             54
#define  Attr_Mechanism_Code                55
#define  Attr_Mod                           56
#define  Attr_Model_Emin                    57
#define  Attr_Model_Epsilon                 58
#define  Attr_Model_Mantissa                59
#define  Attr_Model_Small                   60
#define  Attr_Modulus                       61
#define  Attr_Null_Parameter                62
#define  Attr_Object_Size                   63
#define  Attr_Partition_ID                  64
#define  Attr_Passed_By_Reference           65
#define  Attr_Pool_Address                  66
#define  Attr_Pos                           67
#define  Attr_Position                      68
#define  Attr_Range                         69
#define  Attr_Range_Length                  70
#define  Attr_Round                         71
#define  Attr_Safe_Emax                     72
#define  Attr_Safe_First                    73
#define  Attr_Safe_Large                    74
#define  Attr_Safe_Last                     75
#define  Attr_Safe_Small                    76
#define  Attr_Scale                         77
#define  Attr_Scaling                       78
#define  Attr_Signed_Zeros                  79
#define  Attr_Size                          80
#define  Attr_Small                         81
#define  Attr_Storage_Size                  82
#define  Attr_Storage_Unit                  83
#define  Attr_Stream_Size                   84
#define  Attr_Tag                           85
#define  Attr_Target_Name                   86
#define  Attr_Terminated                    87
#define  Attr_To_Address                    88
#define  Attr_Type_Class                    89
#define  Attr_UET_Address                   90
#define  Attr_Unbiased_Rounding             91
#define  Attr_Unchecked_Access              92
#define  Attr_Unconstrained_Array           93
#define  Attr_Universal_Literal_String      94
#define  Attr_Unrestricted_Access           95
#define  Attr_VADS_Size                     96
#define  Attr_Val                           97
#define  Attr_Valid                         98
#define  Attr_Value_Size                    99
#define  Attr_Version                      100
#define  Attr_Wchar_T_Size                 101
#define  Attr_Wide_Wide_Width              102
#define  Attr_Wide_Width                   103
#define  Attr_Width                        104
#define  Attr_Word_Size                    105

#define  Attr_Adjacent                     106
#define  Attr_Ceiling                      107
#define  Attr_Copy_Sign                    108
#define  Attr_Floor                        109
#define  Attr_Fraction                     110
#define  Attr_Image                        111
#define  Attr_Input                        112
#define  Attr_Machine                      113
#define  Attr_Max                          114
#define  Attr_Min                          115
#define  Attr_Model                        116
#define  Attr_Pred                         117
#define  Attr_Remainder                    118
#define  Attr_Rounding                     119
#define  Attr_Succ                         120
#define  Attr_Truncation                   121
#define  Attr_Value                        122
#define  Attr_Wide_Image                   123
#define  Attr_Wide_Wide_Image              124
#define  Attr_Wide_Value                   125
#define  Attr_Wide_Wide_Value              126

#define  Attr_Output                       127
#define  Attr_Read                         128
#define  Attr_Write                        129

#define  Attr_Elab_Body                    130
#define  Attr_Elab_Spec                    131
#define  Attr_Storage_Pool                 132

#define  Attr_Base                         133
#define  Attr_Class                        134

/* Define the function to check if a Name_Id value is a valid pragma */

#define Is_Pragma_Name snames__is_pragma_name
extern Boolean Is_Pragma_Name (Name_Id);

/* Define the function to return one of the numeric values below.  Note
   that it actually returns a char since an enumeration value of less
   than 256 entries is represented that way in Ada.  The operand is a Chars
   field value.  */

#define Get_Pragma_Id snames__get_pragma_id
extern unsigned char Get_Pragma_Id (int);

/* Define the numeric values for the pragmas. */

/* Configuration pragmas first */

#define  Pragma_Ada_83                        0
#define  Pragma_Ada_95                        1
#define  Pragma_Ada_05                        2
#define  Pragma_C_Pass_By_Copy                3
#define  Pragma_Compile_Time_Warning          4
#define  Pragma_Component_Alignment           5
#define  Pragma_Convention_Identifier         6
#define  Pragma_Detect_Blocking               7
#define  Pragma_Discard_Names                 8
#define  Pragma_Elaboration_Checking          9
#define  Pragma_Eliminate                    10
#define  Pragma_Explicit_Overriding          11
#define  Pragma_Extend_System                12
#define  Pragma_Extensions_Allowed           13
#define  Pragma_External_Name_Casing         14
#define  Pragma_Float_Representation         15
#define  Pragma_Initialize_Scalars           16
#define  Pragma_Interrupt_State              17
#define  Pragma_License                      18
#define  Pragma_Locking_Policy               19
#define  Pragma_Long_Float                   20
#define  Pragma_No_Run_Time                  21
#define  Pragma_No_Strict_Aliasing           22
#define  Pragma_Normalize_Scalars            23
#define  Pragma_Polling                      24
#define  Pragma_Persistent_Data              25
#define  Pragma_Persistent_Object            26
#define  Pragma_Profile                      27
#define  Pragma_Profile_Warnings             28
#define  Pragma_Propagate_Exceptions         29
#define  Pragma_Queuing_Policy               30
#define  Pragma_Ravenscar                    31
#define  Pragma_Restricted_Run_Time          32
#define  Pragma_Restrictions                 33
#define  Pragma_Restriction_Warnings         34
#define  Pragma_Reviewable                   35
#define  Pragma_Source_File_Name             36
#define  Pragma_Source_File_Name_Project     37
#define  Pragma_Style_Checks                 38
#define  Pragma_Suppress                     39
#define  Pragma_Suppress_Exception_Locations 40
#define  Pragma_Task_Dispatching_Policy      41
#define  Pragma_Universal_Data               42
#define  Pragma_Unsuppress                   43
#define  Pragma_Use_VADS_Size                44
#define  Pragma_Validity_Checks              45
#define  Pragma_Warnings                     46

/* Remaining pragmas */

#define  Pragma_Abort_Defer                  47
#define  Pragma_All_Calls_Remote             48
#define  Pragma_Annotate                     49
#define  Pragma_Assert                       50
#define  Pragma_Asynchronous                 51
#define  Pragma_Atomic                       52
#define  Pragma_Atomic_Components            53
#define  Pragma_Attach_Handler               54
#define  Pragma_Comment                      55
#define  Pragma_Common_Object                56
#define  Pragma_Complex_Representation       57
#define  Pragma_Controlled                   58
#define  Pragma_Convention                   59
#define  Pragma_CPP_Class                    60
#define  Pragma_CPP_Constructor              61
#define  Pragma_CPP_Virtual                  62
#define  Pragma_CPP_Vtable                   63
#define  Pragma_Debug                        64
#define  Pragma_Elaborate                    65
#define  Pragma_Elaborate_All                66
#define  Pragma_Elaborate_Body               67
#define  Pragma_Export                       68
#define  Pragma_Export_Exception             69
#define  Pragma_Export_Function              70
#define  Pragma_Export_Object                71
#define  Pragma_Export_Procedure             72
#define  Pragma_Export_Value                 73
#define  Pragma_Export_Valued_Procedure      74
#define  Pragma_External                     75
#define  Pragma_Finalize_Storage_Only        76
#define  Pragma_Ident                        77
#define  Pragma_Import                       78
#define  Pragma_Import_Exception             79
#define  Pragma_Import_Function              80
#define  Pragma_Import_Object                81
#define  Pragma_Import_Procedure             82
#define  Pragma_Import_Valued_Procedure      83
#define  Pragma_Inline                       84
#define  Pragma_Inline_Always                85
#define  Pragma_Inline_Generic               86
#define  Pragma_Inspection_Point             87
#define  Pragma_Interface_Name               88
#define  Pragma_Interrupt_Handler            89
#define  Pragma_Interrupt_Priority           90
#define  Pragma_Java_Constructor             91
#define  Pragma_Java_Interface               92
#define  Pragma_Keep_Names                   93
#define  Pragma_Link_With                    94
#define  Pragma_Linker_Alias                 95
#define  Pragma_Linker_Options               96
#define  Pragma_Linker_Section               97
#define  Pragma_List                         98
#define  Pragma_Machine_Attribute            99
#define  Pragma_Main                        100
#define  Pragma_Main_Storage                101
#define  Pragma_Memory_Size                 102
#define  Pragma_No_Return                   103
#define  Pragma_Obsolescent                 104
#define  Pragma_Optimize                    105
#define  Pragma_Optional_Overriding         106
#define  Pragma_Pack                        107
#define  Pragma_Page                        108
#define  Pragma_Passive                     109
#define  Pragma_Preelaborate                110
#define  Pragma_Priority                    111
#define  Pragma_Psect_Object                112
#define  Pragma_Pure                        113
#define  Pragma_Pure_Function               114
#define  Pragma_Remote_Call_Interface       115
#define  Pragma_Remote_Types                116
#define  Pragma_Share_Generic               117
#define  Pragma_Shared                      118
#define  Pragma_Shared_Passive              119
#define  Pragma_Source_Reference            120
#define  Pragma_Stream_Convert              121
#define  Pragma_Subtitle                    122
#define  Pragma_Suppress_All                123
#define  Pragma_Suppress_Debug_Info         124
#define  Pragma_Suppress_Initialization     125
#define  Pragma_System_Name                 126
#define  Pragma_Task_Info                   127
#define  Pragma_Task_Name                   128
#define  Pragma_Task_Storage                129
#define  Pragma_Thread_Body                 130
#define  Pragma_Time_Slice                  131
#define  Pragma_Title                       132
#define  Pragma_Unchecked_Union             133
#define  Pragma_Unimplemented_Unit          134
#define  Pragma_Unreferenced                135
#define  Pragma_Unreserve_All_Interrupts    136
#define  Pragma_Volatile                    137
#define  Pragma_Volatile_Components         138
#define  Pragma_Weak_External               139

/* The following are deliberately out of alphabetical order, see Snames */

#define  Pragma_AST_Entry                   140
#define  Pragma_Interface                   141
#define  Pragma_Storage_Size                142
#define  Pragma_Storage_Unit                143

/* Define the numeric values for the conventions.  */

#define  Convention_Ada                      0
#define  Convention_Intrinsic                1
#define  Convention_Entry                    2
#define  Convention_Protected                3
#define  Convention_Assembler                4
#define  Convention_C                        5
#define  Convention_COBOL                    6
#define  Convention_CPP                      7
#define  Convention_Fortran                  8
#define  Convention_Java                     9
#define  Convention_Stdcall                 10
#define  Convention_Stubbed                 11

/* End of snames.h (C version of Snames package spec) */
