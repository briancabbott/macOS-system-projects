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

#include "cci_common.h"
#include "cci_array_internal.h"

/* ------------------------------------------------------------------------ */

struct cci_array_d {
    cci_array_object_t *objects;
    cc_uint64 count;
    cc_uint64 max_count;
    
    cci_array_object_release_t object_release;
};

struct cci_array_d cci_array_initializer = { NULL, 0, 0, NULL };

#define CCI_ARRAY_COUNT_INCREMENT 16

/* ------------------------------------------------------------------------ */

static cc_int32 cci_array_resize (cci_array_t io_array,
                                  cc_uint64   in_new_count)
{
    cc_int32 err = ccNoError;
    cc_uint64 new_max_count = 0;
    cci_array_object_t *objects = NULL;
    
    if (!io_array) { err = cci_check_error (ccErrBadParam); }
    
    if (!err) {
        cc_uint64 old_max_count = io_array->max_count;
        new_max_count = io_array->max_count;
        
        if (in_new_count > old_max_count) {
            /* Expand the array */
            while (in_new_count > new_max_count) {
                new_max_count += CCI_ARRAY_COUNT_INCREMENT;
            }
            
        } else if ((in_new_count + CCI_ARRAY_COUNT_INCREMENT) < old_max_count) {
            /* Shrink the array, but never drop below CC_ARRAY_COUNT_INCREMENT */
            while ((in_new_count + CCI_ARRAY_COUNT_INCREMENT) < new_max_count &&
                   (new_max_count > CCI_ARRAY_COUNT_INCREMENT)) {
                new_max_count -= CCI_ARRAY_COUNT_INCREMENT;
            }
        }
    }
    
    if (!err) {
        objects = io_array->objects;
        
        if (!objects) {
            objects = malloc (new_max_count * sizeof (*objects));
        } else {
            objects = realloc (objects, new_max_count * sizeof (*objects));
        }
        if (!objects) { err = cci_check_error (ccErrNoMem); }
    }
    
    if (!err) {
        io_array->objects = objects;
        io_array->max_count = new_max_count;
    }
    
    return cci_check_error (err); 
}

#pragma mark -

/* ------------------------------------------------------------------------ */

cc_int32 cci_array_new (cci_array_t                *out_array,
                        cci_array_object_release_t  in_array_object_release)
{
    cc_int32 err = ccNoError;
    cci_array_t array = NULL;
    
    if (!out_array) { err = cci_check_error (ccErrBadParam); }
    
    if (!err) {
        array = malloc (sizeof (*array));
        if (array) { 
            *array = cci_array_initializer;
            array->object_release = in_array_object_release;
        } else {
            err = cci_check_error (ccErrNoMem); 
        }
    }
    
    if (!err) {
        *out_array = array;
        array = NULL;
    }
    
    cci_array_release (array);
    
    return cci_check_error (err);    
}

/* ------------------------------------------------------------------------ */

cc_int32 cci_array_release (cci_array_t io_array)
{
    cc_int32 err = ccNoError;
    
    if (!io_array) { err = ccErrBadParam; }
    
    if (!err) {
        cc_uint64 i;
        
        for (i = 0; i < io_array->count; i++) {
            io_array->object_release (io_array->objects[i]);
        }
        free (io_array->objects);
        free (io_array);
    }
    
    return err;        
}

/* ------------------------------------------------------------------------ */

cc_uint64 cci_array_count (cci_array_t in_array)
{
    return in_array ? in_array->count : 0;
}

/* ------------------------------------------------------------------------ */

cci_array_object_t cci_array_object_at_index (cci_array_t io_array,
                                              cc_uint64   in_position)
{
    if (io_array && in_position < io_array->count) {
        return io_array->objects[in_position];
    } else {
        if (!io_array) {
            cci_debug_printf ("%s() got NULL array", __FUNCTION__);
        } else {
            cci_debug_printf ("%s() got bad index %lld (count = %lld)", __FUNCTION__, 
                              in_position, io_array->count);
        }     
        return NULL;
    }
}

#pragma mark -
    
/* ------------------------------------------------------------------------ */

cc_int32 cci_array_insert (cci_array_t        io_array,
                           cci_array_object_t in_object,
                           cc_uint64          in_position)
{
    cc_int32 err = ccNoError;
    
    if (!io_array ) { err = cci_check_error (ccErrBadParam); }
    if (!in_object) { err = cci_check_error (ccErrBadParam); }
    
    if (!err) {
        /* Don't try to insert past the end and don't overflow the array */
        if (in_position > io_array->count || io_array->count == UINT64_MAX) {
            err = cci_check_error (ccErrBadParam);
        }
    }
    
    if (!err) {
        err = cci_array_resize (io_array, io_array->count + 1);
    }
    
    if (!err) {
        unsigned char **objects = (unsigned char **)io_array->objects;
        cc_uint64 move_count = io_array->count - in_position;
        
        if (move_count > 0) {
            memmove (&objects[in_position + 1], &objects[in_position],
                     move_count * sizeof (*objects));
        }
        
        objects[in_position] = in_object;
        io_array->count++;
    }
    
    return cci_check_error (err);    
}

/* ------------------------------------------------------------------------ */

cc_int32 cci_array_remove (cci_array_t io_array,
                           cc_uint64   in_position)
{
    cc_int32 err = ccNoError;
    
    if (!io_array) { err = cci_check_error (ccErrBadParam); }
    
    if (!err && in_position >= io_array->count) {
        err = cci_check_error (ccErrBadParam);
    }
    
    if (!err) {
        unsigned char **objects = (unsigned char **)io_array->objects;
        cc_uint64 move_count = io_array->count - in_position - 1;
        cci_array_object_t object = objects[in_position];
        
        if (move_count > 0) {
            memmove (&objects[in_position], &objects[in_position + 1],
                     move_count * sizeof (*objects));
        }
        
        io_array->object_release (object);
        io_array->count--;
        
        cci_array_resize (io_array, io_array->count);
    }
    
    return cci_check_error (err);    
}

/* ------------------------------------------------------------------------ */

cc_int32 cci_array_push_front (cci_array_t io_array,
                               cc_uint64   in_position)
{
    cc_int32 err = ccNoError;
    
    if (!io_array) { err = cci_check_error (ccErrBadParam); }
    
    if (!err && in_position >= io_array->count) {
        err = cci_check_error (ccErrBadParam);
    }
    
    if (!err) {
        cc_uint64 move_count = in_position;
        
        if (move_count > 0) {
            unsigned char **objects = (unsigned char **)io_array->objects;
            cci_array_object_t object = objects[in_position];

            memmove (&objects[1], &objects[0], move_count * sizeof (*objects));
            objects[0] = object;
        }
        
    }
    
    return cci_check_error (err);    
}

