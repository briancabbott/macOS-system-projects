------------------------------------------------------------------------------
--                                                                          --
--                GNU ADA RUN-TIME LIBRARY (GNARL) COMPONENTS               --
--                                                                          --
--            S Y S T E M . I N T E R R U P T _ M A N A G E M E N T         --
--                                                                          --
--                                  S p e c                                 --
--                                                                          --
--          Copyright (C) 1992-2003 Free Software Foundation, Inc.          --
--                                                                          --
-- GNARL is free software; you can  redistribute it  and/or modify it under --
-- terms of the  GNU General Public License as published  by the Free Soft- --
-- ware  Foundation;  either version 2,  or (at your option) any later ver- --
-- sion. GNARL is distributed in the hope that it will be useful, but WITH- --
-- OUT ANY WARRANTY;  without even the  implied warranty of MERCHANTABILITY --
-- or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License --
-- for  more details.  You should have  received  a copy of the GNU General --
-- Public License  distributed with GNARL; see file COPYING.  If not, write --
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
-- GNARL was developed by the GNARL team at Florida State University.       --
-- Extensive contributions were provided by Ada Core Technologies, Inc.     --
--                                                                          --
------------------------------------------------------------------------------

--  This is the VxWorks version of this package.

--  This package encapsulates and centralizes information about all
--  uses of interrupts (or signals), including the target-dependent
--  mapping of interrupts (or signals) to exceptions.

--  Unlike the original design, System.Interrupt_Management can only
--  be used for tasking systems.

--  PLEASE DO NOT remove the Elaborate_Body pragma from this package.
--  Elaboration of this package should happen early, as most other
--  initializations depend on it. Forcing immediate elaboration of
--  the body also helps to enforce the design assumption that this
--  is a second-level package, just one level above System.OS_Interface
--  with no cross-dependencies.

--  PLEASE DO NOT put any subprogram declarations with arguments of
--  type Interrupt_ID into the visible part of this package. The type
--  Interrupt_ID is used to derive the type in Ada.Interrupts, and
--  adding more operations to that type would be illegal according
--  to the Ada Reference Manual. This is the reason why the signals
--  sets are implemeneted using visible arrays rather than functions.

with System.OS_Interface;
--  used for sigset_t

with Interfaces.C;
--  used for int

package System.Interrupt_Management is

   pragma Elaborate_Body;

   type Interrupt_Mask is limited private;

   type Interrupt_ID is new Interfaces.C.int
     range 0 .. System.OS_Interface.Max_Interrupt;

   type Interrupt_Set is array (Interrupt_ID) of Boolean;

   subtype Signal_ID is Interrupt_ID
     range 0 .. Interfaces.C."-" (System.OS_Interface.NSIG, 1);

   type Signal_Set is array (Signal_ID) of Boolean;

   --  The following objects serve as constants, but are initialized
   --  in the body to aid portability.  This permits us to use more
   --  portable names for interrupts, where distinct names may map to
   --  the same interrupt ID value.
   --
   --  For example, suppose SIGRARE is a signal that is not defined on
   --  all systems, but is always reserved when it is defined. If we
   --  have the convention that ID zero is not used for any "real"
   --  signals, and SIGRARE = 0 when SIGRARE is not one of the locally
   --  supported signals, we can write
   --     Reserved (SIGRARE) := true;
   --  and the initialization code will be portable.

   Abort_Task_Signal : Signal_ID;
   --  The signal that is used to implement task abortion if
   --  an interrupt is used for that purpose. This is one of the
   --  reserved signals.

   Keep_Unmasked : Signal_Set := (others => False);
   --  Keep_Unmasked (I) is true iff the signal I is one that must
   --  that must be kept unmasked at all times, except (perhaps) for
   --  short critical sections. This includes signals that are
   --  mapped to exceptions, but may also include interrupts
   --  (e.g. timer) that need to be kept unmasked for other
   --  reasons. Where signal masking is per-task, the signal should be
   --  unmasked in ALL TASKS.

   Reserve : Interrupt_Set := (others => False);
   --  Reserve (I) is true iff the interrupt I is one that cannot be
   --  permitted to be attached to a user handler. The possible reasons
   --  are many. For example, it may be mapped to an exception used to
   --  implement task abortion, or used to implement time delays.

   procedure Initialize_Interrupts;
   --  On systems where there is no signal inheritance between tasks (e.g
   --  VxWorks, GNU/LinuxThreads), this procedure is used to initialize
   --  interrupts handling in each task. Otherwise this function should
   --  only be called by initialize in this package body.

private
   type Interrupt_Mask is new System.OS_Interface.sigset_t;
   --  In some implementation Interrupt_Mask can be represented
   --  as a linked list.

end System.Interrupt_Management;
