/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Portions of this software have been released under the following terms:
 *
 * (c) Copyright 1989-1993 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989-1993 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989-1993 DIGITAL EQUIPMENT CORPORATION
 *
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 * permission to use, copy, modify, and distribute this file for any
 * purpose is hereby granted without fee, provided that the above
 * copyright notices and this notice appears in all source code copies,
 * and that none of the names of Open Software Foundation, Inc., Hewlett-
 * Packard Company or Digital Equipment Corporation be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  Neither Open Software
 * Foundation, Inc., Hewlett-Packard Company nor Digital
 * Equipment Corporation makes any representations about the suitability
 * of this software for any purpose.
 *
 * Copyright (c) 2007, Novell, Inc. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Novell Inc. nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
**
**  NAME:
**
**      NAMDUMP.C
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**  This is the dumper for the name table.
**
**  VERSION: DCE 1.0
**
*/

#include <nidl.h>
#include <ctype.h>

#include <errors.h>
#include <nametbl.h>
#include <namtbpvt.h>
#include <nidlmsg.h>

extern NAMETABLE_id_t NAMETABLE_root;
extern NAMETABLE_temp_name_t * NAMETABLE_temp_chain;

/******************************************************************************/
/*                                                                            */
/*                  N A M E T A B L E          D U M P E R S                  */
/*                                                                            */
/******************************************************************************/

/*
 * Function: Dumps the binding information for a node of the nametable tree.
 *
 * Inputs:   Node of the tree to be dumped.
 *
 * Outputs:  None.
 *
 * Functional Value: None.
 *
 * Notes:    None.
 *
 */

static void NAMETABLE_dump_bindings_4_node
(
    NAMETABLE_binding_n_t * pp
)
{
    NAMETABLE_binding_n_t * p;

    p = pp;

    while (p != NULL) {
        printf ("\tBinding node at: %p \n", p);
        printf ("\t    bindingLevel: %d\n", p->bindingLevel);
        printf ("\t    theBinding: %p \n", p->theBinding);
        printf ("\t    nextBindingThisLevel: %p \n", p->nextBindingThisLevel);
        printf ("\t    oldBinding: %p \n", p->oldBinding);
        printf ("\t    boundBy: \"%s\" ( %p )\n\n", p->boundBy->id, p->boundBy);
        if (p->oldBinding != NULL) {
            printf ("\n");
        };
        p = p->oldBinding;
    };
}

/*
 * Function: Dumps a node of the nametable tree.
 *
 * Inputs:   Node of the tree to be dumped.
 *
 * Outputs:  Text is output to stdout.
 *
 * Functional Value: None.
 *
 * Notes:    None.
 *
 */

static void NAMETABLE_dump_node
(
    NAMETABLE_id_t node
)
{
    printf ("\n\"%s\" ( %p ) :\n",        /* "FOO" (0023ad8C) : */
            node->id,                   /* The id string */
            (char *) node );            /* The address of the node */

    if (node->parent != NULL) {
        printf ("        Parent: ( %p ) \"%s\"\n", /*     Parent:  ( 01234abc ) "bar" */
                node->parent,             /* The address of the parent */
                node->parent->id);        /* The id string of parent */
    } else {
        printf ("        *** NAMETABLE ROOT ***\n");     /* Handle the NULL case. */
    };

    if (node->left != NULL) {
        printf ("        Left:  ( %p ) \"%s\"\n", /*     Left:  ( 01234abc ) "bar" */
                node->left,             /* The address of the left child */
                node->left->id);        /* The id string of l. child */
    } else {
        printf ("        Left:  NULL\n");     /* Handle the NULL case. */
    };

    if (node->right != NULL) {
        printf ("        Right: ( %p ) \"%s\"\n", /*     Right: (01234abc) "bar" */
                node->right,            /* The address of the right child */
                node->right->id);       /* The id string of r. child */
    } else {
        printf ("        Right: NULL\n");   /* Handle the NULL case. */
    };

    if (node->bindings != NULL) {
        printf ("    Head of binding chain : %p \n",
                node->bindings);
        printf ("    Binding information for \"%s\"\n",
                node->id);
        NAMETABLE_dump_bindings_4_node (node->bindings);
    } else {
        printf ("    No binding chain\n");
    };

    if (node->tagBinding != NULL) {
        printf ("    Structure with this tag: %p \n",
                node->tagBinding);
        printf ("    Tag binding information for \"%s\"\n",
                node->id);
        NAMETABLE_dump_bindings_4_node (node->tagBinding);
    } else {
        printf ("    No structures with this tag\n");
    };
}

/*
 * Function: Recursively dumps all the nodes of a nametable tree.
 *           First dumps the left subtree bottom up, then the root node,
 *           then the right subtree, resulting in an alphabetical dump.
 *
 * Inputs:   Root node of the tree to be dumped.
 *
 * Outputs:  None.
 *
 * Functional Value: None.
 *
 * Notes:    None.
 *
 */

static void NAMETABLE_dump_nodes
(
    NAMETABLE_id_t node
)
{
    if (node->left != NULL) {
        NAMETABLE_dump_nodes (node->left);
    };

    NAMETABLE_dump_node (node);

    if (node->right != NULL) {
        NAMETABLE_dump_nodes (node->right);
    };
}

/*
 * Function: Dump the list of temporary name table nodes.
 *
 * Inputs:   NAMETABLE_temp_chain (Implicit)
 *
 * Outputs:
 *
 * Functional Value:
 *
 * Notes:
 *
 */

static void NAMETABLE_dump_temp_node_list (void)
{
NAMETABLE_temp_name_t * tb;

    if (!NAMETABLE_temp_chain) {
        printf ("\n\nThere are no temporary names.\n");
        return;
    } else {
        printf ("\n\nTemporary name chain:\n");
    }

    for (tb = NAMETABLE_temp_chain; tb; tb = tb->next) {
        printf ("    Chain block: %p NT node ( %p ): \"%s\"\n",
            tb, tb->node, tb->node->id );
    }

}

/*
 * Function: Dump a name table in human-readable form.
 *
 * Inputs:   name_table - the table to be dumped. (Implicit)
 *
 * Outputs:
 *
 * Functional Value:
 *
 * Notes:
 *
 */

void NAMETABLE_dump_tab (void)
{
    NAMETABLE_dump_nodes( NAMETABLE_root );
    NAMETABLE_dump_temp_node_list();
}
