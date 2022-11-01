/*
 * $Header$
 *
 * Copyright 2006 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include <krb5.h>
#include <gssapi/gssapi.h>
#include "kim_private.h"

/* ------------------------------------------------------------------------ */

struct kim_identity_opaque {
    krb5_context          context;
    krb5_principal        principal;
};

struct kim_identity_opaque kim_identity_initializer = { NULL, NULL };

/* ------------------------------------------------------------------------ */

static inline kim_error_t kim_identity_allocate (kim_identity_t *out_identity)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_identity_t identity = NULL;
    
    if (out_identity == NULL) { err = param_error (1, "out_identity", "NULL"); }
    
    if (!err) {
        identity = malloc (sizeof (*identity));
        if (!identity) { err = os_error (errno); }
    }
    
    if (!err) {
        *identity = kim_identity_initializer;
        *out_identity = identity;
        identity = NULL;
    }
    
    kim_identity_free (&identity);
    
    return check_error (err);    
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_identity_create_from_string (kim_identity_t *out_identity,
                                             kim_string_t    in_string)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_identity_t identity = NULL;
    
    if (out_identity == NULL) { err = param_error (1, "out_identity", "NULL"); }
    if (in_string    == NULL) { err = param_error (2, "in_string", "NULL"); }
        
    if (!err) {
        err = kim_identity_allocate (&identity);
    }
    
    if (!err) {
        err = krb5_error (krb5_init_context (&identity->context));
    }
    
    if (!err) {
        krb5_error_code code = krb5_parse_name (identity->context, in_string, &identity->principal);
        if (code == KRB5_PARSE_MALFORMED) {
            err = kim_error_create_from_code (KIM_BAD_PRINCIPAL_STRING_ECODE, in_string);
        } else if (code) {
            err = krb5_error (code);
        }
    }
    
    if (!err) {
#warning Run translator here
    }
    
    if (!err) {
        *out_identity = identity;
        identity = NULL;
    }
    
    if (identity != NULL) { kim_identity_free (&identity); }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_identity_create_from_components (kim_identity_t *out_identity,
                                                 kim_string_t    in_realm, 
                                                 kim_string_t    in_1st_component,
                                                 ...)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_identity_t identity = NULL;
    krb5_principal_data principal_data;  /* allocated by KIM so can't be returned */
    
    if (out_identity     == NULL) { err = param_error (1, "out_identity", "NULL"); }
    if (in_realm         == NULL) { err = param_error (2, "in_realm", "NULL"); }
    if (in_1st_component == NULL) { err = param_error (3, "in_1st_component", "NULL"); }
    
    if (!err) {
        err = kim_identity_allocate (&identity);
    }
    
    if (!err) {
        err = krb5_error (krb5_init_context (&identity->context));
    }
        
    if (!err) {
        va_list args;
        kim_count_t component_count = 1;

        va_start (args, in_1st_component);
        while (va_arg (args, kim_string_t) != NULL) { component_count++; }
        va_end (args);
        
        principal_data.length = component_count;
        principal_data.data = (krb5_data *) malloc (component_count * sizeof (krb5_data));
        if (!principal_data.data) { err = os_error (errno); }
    }   
    
    if (!err) {
        va_list args;
        krb5_int32 i;
            
        krb5_princ_set_realm_length (context, &principal_data, strlen (in_realm));
        krb5_princ_set_realm_data (context, &principal_data, (char *) in_realm);
        
        va_start (args, in_1st_component);
        for (i = 0; !err && (i < principal_data.length); i++) {
            kim_string_t component = NULL;
            if (i == 0) {
                err = kim_string_copy (&component, in_1st_component);
            } else {
                err = kim_string_copy (&component, va_arg (args, kim_string_t));
            }
            
            if (!err) {
                principal_data.data[i].data = (char *) component;
                principal_data.data[i].length = strlen (component);
            }
        }            
        va_end (args);
    }
        
    if (!err) {
#warning Need a better way to create a krb5 principal
        /* make a copy that has actually been allocated by the krb5 
        * library so krb5_free_principal can be called on it */
        err = krb5_error (krb5_copy_principal (identity->context, &principal_data, &identity->principal));
    }    
    
    if (!err) {
#warning Run translator here
    }
    
    if (!err) {
        *out_identity = identity;
        identity = NULL;
    }
    
    if (principal_data.data != NULL) { 
        krb5_int32 i;
        
        for (i = 0; i < principal_data.length; i++) {
            kim_string_t component = principal_data.data[i].data;
            kim_string_free (&component);
        }
        free (principal_data.data); 
    }
    kim_identity_free (&identity);
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_identity_create_from_krb5_principal (kim_identity_t *out_identity,
                                                     krb5_context    in_krb5_context,
                                                     krb5_principal  in_krb5_principal)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_identity_t identity = NULL;
    
    if (out_identity      == NULL) { err = param_error (1, "out_identity", "NULL"); }
    if (in_krb5_principal == NULL) { err = param_error (2, "in_krb5_principal", "NULL"); }
    if (in_krb5_context   == NULL) { err = param_error (3, "in_krb5_context", "NULL"); }
    
    if (!err) {
        err = kim_identity_allocate (&identity);
    }
    
    if (!err) {
        err = krb5_error (krb5_init_context (&identity->context));
    }

    if (!err) {
        err = krb5_error (krb5_copy_principal (identity->context, in_krb5_principal, 
                                               &identity->principal));
    }
    
    if (!err) {
#warning Run translator here
    }
    
    if (!err) {
        *out_identity = identity;
        identity = NULL;
    }
    
    kim_identity_free (&identity);
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_identity_copy (kim_identity_t *out_identity,
                               kim_identity_t  in_identity)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_identity_t identity = KIM_IDENTITY_ANY;
    
    if (out_identity == NULL) { err = param_error (1, "out_identity", "NULL"); }

    if (!err && in_identity != KIM_IDENTITY_ANY) {
        err = kim_identity_allocate (&identity);
        
        if (!err) {
            err = krb5_error (krb5_init_context (&identity->context));
        }
        
        if (!err) {
            err = krb5_error (krb5_copy_principal (identity->context, in_identity->principal, 
                                                   &identity->principal));
        }
    }

    if (!err) {
        *out_identity = identity;
        identity = NULL;
    }
        
    kim_identity_free (&identity);
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_identity_compare (kim_identity_t    in_identity,
                                  kim_identity_t    in_compare_to_identity,
                                  kim_comparison_t *out_comparison)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_identity            == NULL) { err = param_error (1, "in_identity", "NULL"); }
    if (in_compare_to_identity == NULL) { err = param_error (2, "in_compare_to_identity", "NULL"); }
    if (out_comparison         == NULL) { err = param_error (3, "out_comparison", "NULL"); }
    
    if (!err) {
        if (krb5_principal_compare (in_identity->context, 
                                    in_identity->principal, 
                                    in_compare_to_identity->principal)) {
            *out_comparison = 0;
        } else {
            kim_string_t string = NULL;
            kim_string_t compare_to_string = NULL;
            
            err = kim_identity_get_string (in_identity, &string);
            
            if (!err) {
                err = kim_identity_get_string (in_compare_to_identity, &compare_to_string);
            }
            
            if (!err) {
                err = kim_string_compare (string, compare_to_string, out_comparison);
            }
            
            kim_string_free (&string);
            kim_string_free (&compare_to_string);
        }
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_identity_get_string (kim_identity_t   in_identity,
                                     kim_string_t    *out_string)
{
    kim_error_t err = KIM_NO_ERROR;
    char *unparsed_name = NULL;
    
    if (in_identity == NULL) { err = param_error (1, "in_identity", "NULL"); }
    if (out_string  == NULL) { err = param_error (2, "out_string", "NULL"); }
    
    if (!err) {
        err = krb5_error (krb5_unparse_name (in_identity->context, 
                                             in_identity->principal, 
                                             &unparsed_name));
    }
    
    if (!err) {
        err = kim_string_copy (out_string, unparsed_name);
    }
    
    if (unparsed_name != NULL) { krb5_free_unparsed_name (in_identity->context, unparsed_name); }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_identity_get_display_string (kim_identity_t   in_identity,
                                             kim_string_t    *out_display_string)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_string_t string = NULL;
    
    if (in_identity        == NULL) { err = param_error (1, "in_identity", "NULL"); }
    if (out_display_string == NULL) { err = param_error (2, "out_display_string", "NULL"); }
    
    if (!err) {
        err = kim_identity_get_string (in_identity, &string);
    }
    
    if (!err) {
        kim_count_t i, j;
        kim_count_t length = strlen (string) + 1; /* Copy the '\0' */
        char *display_string = (char *) string; /* so we can modify it */
        
        /* In place copy, skipping escaped separators.
         * Note that we do not want to remove other escaped characters
         * (tab, break, newline, NULL) because they are less readable 
         * when unescaped (and NULL isn't a valid string character).  */
        for (i = 0, j = 0; i < length; i++) {
            if (string[i] == '\\') {
                switch (string[i + 1]) {
                    case '/': /* component separator */
                    case '@': /* realm separator */
                        i++;  /* skip the '\' */
                        continue;
                }
            }
            
            display_string[j++] = string[i]; /* Copy this char */
        } 
        
        *out_display_string = string;
        string = NULL;
    }
    
    if (string != NULL) { kim_string_free (&string); }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_identity_get_realm (kim_identity_t  in_identity,
                                    kim_string_t   *out_realm_string)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_identity      == NULL) { err = param_error (1, "in_identity", "NULL"); }
    if (out_realm_string == NULL) { err = param_error (2, "out_realm_string", "NULL"); }
    
    if (!err) {
        krb5_data *realm = krb5_princ_realm (in_identity->context, in_identity->principal);
        
        err = kim_string_create_from_buffer (out_realm_string, realm->data, realm->length);
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_identity_get_number_of_components (kim_identity_t  in_identity,
                                                   kim_count_t    *out_number_of_components)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_identity              == NULL) { err = param_error (1, "in_identity", "NULL"); }
    if (out_number_of_components == NULL) { err = param_error (2, "out_number_of_components", "NULL"); }
    
    if (!err) {
        *out_number_of_components = krb5_princ_size (in_identity->context, in_identity->principal);
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_identity_get_component_at_index (kim_identity_t  in_identity,
                                                 kim_count_t     in_index,
                                                 kim_string_t   *out_component_string)
{
    kim_error_t err = KIM_NO_ERROR;
    krb5_data *component = NULL;
    
    if (in_identity          == NULL) { err = param_error (1, "in_identity", "NULL"); }
    if (out_component_string == NULL) { err = param_error (3, "out_component_string", "NULL"); }
    
    if (!err) {
        krb5_int32 i = in_index;
        component = krb5_princ_component (in_identity->context, in_identity->principal, i);
        if (!component) { err = kim_error_create_from_code (KIM_BAD_COMPONENT_INDEX_ECODE, i); }
    }
    
    if (!err) {
        err = kim_string_create_from_buffer (out_component_string, component->data, component->length);
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_identity_get_krb5_principal (kim_identity_t  in_identity,
                                             krb5_context    in_krb5_context,
                                             krb5_principal *out_krb5_principal)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_identity        == NULL) { err = param_error (1, "in_identity", "NULL"); }
    if (in_krb5_context    == NULL) { err = param_error (2, "in_krb5_context", "NULL"); }
    if (out_krb5_principal == NULL) { err = param_error (3, "out_krb5_principal", "NULL"); }
    
    if (!err) {
        err = krb5_error (krb5_copy_principal (in_identity->context, 
                                               in_identity->principal, 
                                               out_krb5_principal));
    }    
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_identity_get_gss_name (kim_identity_t  in_identity,
                                       gss_name_t     *out_gss_name)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_identity  == NULL) { err = param_error (1, "in_identity", "NULL"); }
    if (out_gss_name == NULL) { err = param_error (2, "out_gss_name", "NULL"); }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_identity_is_tgt_service (kim_identity_t  in_identity,
                                         kim_boolean_t  *out_is_tgt_service)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_identity        == NULL) { err = param_error (1, "in_identity", "NULL"); }
    if (out_is_tgt_service == NULL) { err = param_error (2, "out_is_tgt_service", "NULL"); }
    
    if (!err) {
        kim_count_t count = krb5_princ_size (in_identity->context, in_identity->principal);
        krb5_data *name = krb5_princ_name (in_identity->context, in_identity->principal);
        
        /* krbtgt/<REALM1>@<REALM2> (usually REALM1 == REALM2, but not always) */
        *out_is_tgt_service = ((count == 2) &&
                               (strlen (KRB5_TGS_NAME) == name->length) &&
                               (strncmp (name->data, KRB5_TGS_NAME, name->length) == 0));
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_identity_change_password (kim_identity_t in_identity,
                                          kim_options_t  in_options)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_identity == NULL) { err = param_error (1, "in_identity", "NULL"); }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_identity_change_password_to_password (kim_identity_t in_identity,
                                                      kim_options_t  in_options,
                                                      kim_string_t   in_new_password)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_identity     == NULL) { err = param_error (1, "in_identity", "NULL"); }
    if (in_new_password == NULL) { err = param_error (3, "in_new_password", "NULL"); }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

void kim_identity_free (kim_identity_t *io_identity)
{
    if (io_identity && *io_identity) { 
        kim_identity_t identity = *io_identity;

        if (identity->context) { 
            if (identity->principal) { 
                krb5_free_principal (identity->context, identity->principal); 
            }
            krb5_free_context (identity->context);
        }
        
        free (identity);
        *io_identity = NULL;
    }
}
