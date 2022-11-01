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

#ifndef KIM_STRING_PRIVATE_H
#define KIM_STRING_PRIVATE_H

#include <kim/kim.h>


kim_error_t kim_string_create_from_format (kim_string_t *out_string, 
                                           kim_string_t  in_format,
                                           ...);

kim_error_code_t kim_string_create_from_format_va_retcode (kim_string_t *out_string, 
                                                           kim_string_t  in_format,
                                                           va_list       in_args);

kim_error_t kim_string_create_from_format_va (kim_string_t *out_string, 
                                              kim_string_t  in_format,
                                              va_list       in_args);

kim_error_t kim_string_create_from_buffer (kim_string_t *out_string, 
                                           const char   *in_buffer, 
                                           kim_count_t   in_length);

kim_error_t kim_string_prepend (kim_string_t *io_string,
                                kim_string_t  in_prefix);

kim_error_t kim_string_append (kim_string_t *io_string, 
                               kim_string_t  in_suffix);

/* OS-specific because it should use UTF8-safe sorting where possible */
kim_error_t kim_os_string_compare (kim_string_t      in_string,
                                   kim_string_t      in_compare_to_string,
                                   kim_comparison_t *out_comparison);

#endif /* KIM_STRING_PRIVATE_H */
