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

#include "kim_private.h"


/* ------------------------------------------------------------------------ */

static inline kim_error_t kim_string_allocate (kim_string_t *out_string,
                                               kim_count_t   in_length)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_string_t string = NULL;
    
    if (out_string == NULL) { err = param_error (1, "out_string", "NULL"); }
    
    if (!err) {
        string = calloc (in_length, sizeof (char *));
        if (!string) { err = os_error (errno); }
    }
    
    if (!err) {
        *out_string = string;
        string = NULL;
    }
    
    kim_string_free (&string);
    
    return check_error (err);    
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_string_create_from_format (kim_string_t *out_string, 
                                           kim_string_t  in_format,
                                           ...)
{
    kim_error_t err = KIM_NO_ERROR;
    va_list args;
    
    va_start (args, in_format);
    err = kim_string_create_from_format_va (out_string, in_format, args);
    va_end (args);
    
    return check_error (err);    
}

/* ------------------------------------------------------------------------ */

kim_error_code_t kim_string_create_from_format_va_retcode (kim_string_t *out_string, 
                                                           kim_string_t  in_format,
                                                           va_list       in_args)
{
    kim_error_code_t code = KIM_NO_ERROR_ECODE;
    int count = vasprintf ((char **) out_string, in_format, in_args);
    if (count < 0) { code = KIM_OUT_OF_MEMORY_ECODE; }
    
    return code;
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_string_create_from_format_va (kim_string_t *out_string, 
                                              kim_string_t  in_format,
                                              va_list       in_args)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_string_t string = NULL;
    
    if (out_string == NULL) { err = param_error (1, "out_string", "NULL"); }
    if (in_format  == NULL) { err = param_error (2, "in_format", "NULL"); }
    
    if (!err) {
        kim_error_code_t code = kim_string_create_from_format_va_retcode (&string, in_format, in_args);
        if (code) { err = kim_error_create_from_code (code); }
    }
    
    if (!err) {
        *out_string = string;
        string = NULL;
    }
    
    if (string != NULL) { kim_string_free (&string); }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_string_create_from_buffer (kim_string_t *out_string, 
                                           const char   *in_buffer, 
                                           kim_count_t   in_length)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_string_t string = NULL;
    
    if (out_string == NULL) { err = param_error (1, "out_string", "NULL"); }
    if (in_buffer  == NULL) { err = param_error (2, "in_buffer", "NULL"); }
    
    if (!err) {
        err = kim_string_allocate (&string, in_length + 1);
    }
    
    if (!err) {
        memcpy ((char *) string, in_buffer, in_length * sizeof (char));
        *out_string = string;
        string = NULL;
    }
    
    kim_string_free (&string);
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_string_copy (kim_string_t *out_string, 
                             kim_string_t  in_string)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_string_t string = NULL;
    
    if (out_string == NULL) { err = param_error (1, "out_string", "NULL"); }
    if (in_string  == NULL) { err = param_error (2, "in_string", "NULL"); }
        
    if (!err) {
        err = kim_string_allocate (&string, strlen (in_string) + 1);
    }
    
    if (!err) {
        strcpy ((char *) string, in_string);
        *out_string = string;
        string = NULL;
    }
    
    kim_string_free (&string);
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_string_compare (kim_string_t      in_string, 
                                kim_string_t      in_compare_to_string,
                                kim_comparison_t *out_comparison)
{
    return kim_os_string_compare (in_string, in_compare_to_string, out_comparison);
}

/* ------------------------------------------------------------------------ */

void kim_string_free (kim_string_t *io_string)
{
    if (io_string && *io_string) { 
        free ((char *) *io_string);
        *io_string = NULL;
    }
}
