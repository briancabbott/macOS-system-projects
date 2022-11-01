/* Longjump free calls to gdb internal routines.
   Copyright 1999, 2000 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#ifndef WRAPPER_H
#define WRAPPER_H 1
#include "gdb.h"
#include "varobj.h"

struct value;
struct expression;
struct block;

/* Use this struct to pass arguments to wrapper routines. */
struct gdb_wrapper_arguments;

extern int gdb_parse_exp_1 (char **, struct block *,
			    int, struct expression **);

extern int gdb_evaluate_expression (struct expression *, struct value **);

extern int gdb_print_expression (struct expression *, struct ui_file *);

extern int gdb_evaluate_type (struct expression *exp, struct value **value);

extern int gdb_value_fetch_lazy (struct value *);

extern int gdb_value_equal (struct value *, struct value *, int *);

extern int gdb_value_assign (struct value *, struct value *, struct value **);

extern int gdb_value_subscript (struct value *, struct value *, struct value **);

extern enum gdb_rc gdb_value_struct_elt (struct ui_out *uiout, struct value **result_ptr,
					 struct value **argp, struct value **args,
					 char *name, int *static_memfuncp, char *err);

extern int gdb_value_ind (struct value *val, struct value ** rval);

extern int
gdb_value_cast (struct type *type, struct value *in_val, struct value **out_val);

extern int gdb_parse_and_eval_type (char *, int, struct type **);

int gdb_varobj_get_value (struct varobj *val1, char **result);

int safe_value_objc_target_type (struct value *val, struct block *block, struct type **dynamic_type);

enum gdb_rc safe_execute_command (struct ui_out *uiout, char *command,
					 int from_tty);
#endif /* WRAPPER_H */
