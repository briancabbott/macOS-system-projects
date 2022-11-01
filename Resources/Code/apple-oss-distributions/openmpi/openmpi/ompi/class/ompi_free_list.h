/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2006 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef OMPI_FREE_LIST_H
#define OMPI_FREE_LIST_H

#include "ompi_config.h"
#include "opal/class/opal_atomic_lifo.h"
#include "opal/threads/threads.h"
#include "opal/threads/condition.h"
#include "ompi/constants.h"
#include "opal/sys/cache.h"

#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif

struct mca_mem_pool_t;

struct ompi_free_list_t
{
    opal_atomic_lifo_t super;
    size_t fl_max_to_alloc;
    size_t fl_num_allocated;
    size_t fl_num_per_alloc;
    size_t fl_num_waiting;
    size_t fl_elem_size;
    size_t fl_header_space;
    size_t fl_alignment;
    opal_class_t* fl_elem_class;
    struct mca_mpool_base_module_t* fl_mpool;
    opal_mutex_t fl_lock;
    opal_condition_t fl_condition; 
    opal_list_t fl_allocations;
};
typedef struct ompi_free_list_t ompi_free_list_t;
OMPI_DECLSPEC OBJ_CLASS_DECLARATION(ompi_free_list_t);

struct ompi_free_list_item_t
{ 
    opal_list_item_t super; 
    void* user_data; 
}; 
typedef struct ompi_free_list_item_t ompi_free_list_item_t; 

OMPI_DECLSPEC OBJ_CLASS_DECLARATION(ompi_free_list_item_t);

struct ompi_free_list_memory_t {
    opal_list_item_t super;
    struct mca_mpool_base_registration_t *registration;
};
typedef struct ompi_free_list_memory_t ompi_free_list_memory_t;
OMPI_DECLSPEC OBJ_CLASS_DECLARATION(ompi_free_list_memory_t);

/**
 * Initialize a free list.
 *
 * @param free_list (IN)           Free list.
 * @param element_size (IN)        Size of each element.
 * @param element_class (IN)       opal_class_t of element - used to initialize allocated elements.
 * @param num_elements_to_alloc    Initial number of elements to allocate.
 * @param max_elements_to_alloc    Maximum number of elements to allocate.
 * @param num_elements_per_alloc   Number of elements to grow by per allocation.
 * @param mpool                    Optional memory pool for allocation.s
 */
 
OMPI_DECLSPEC int ompi_free_list_init_ex(
    ompi_free_list_t *free_list, 
    size_t element_size,
    size_t header_size,
    size_t alignment,
    opal_class_t* element_class,
    int num_elements_to_alloc,
    int max_elements_to_alloc,
    int num_elements_per_alloc,
    struct mca_mpool_base_module_t*);

static inline int ompi_free_list_init(
    ompi_free_list_t *free_list, 
    size_t element_size,
    opal_class_t* element_class,
    int num_elements_to_alloc,
    int max_elements_to_alloc,
    int num_elements_per_alloc,
    struct mca_mpool_base_module_t* mpool)
{
    return ompi_free_list_init_ex(free_list, element_size, 0, CACHE_LINE_SIZE,
            element_class, num_elements_to_alloc, max_elements_to_alloc,
            num_elements_per_alloc, mpool);
}


OMPI_DECLSPEC int ompi_free_list_grow(ompi_free_list_t* flist, size_t num_elements);

/* Grow the free list to be *at least* size elements.  This function
   will not shrink the list if it is already larger than size and may
   grow it past size if necessary (it will grow in
   num_elements_per_alloc chunks) */
OMPI_DECLSPEC int ompi_free_list_resize(ompi_free_list_t *flist, size_t size);

/* Allow to walk through the all allocated items. Not thread-safe, not
 * protected, this function should never be used except for debugging purposes.
 * The position should never be NULL, and it should be set to {NULL, NULL} 
 * for the first call.
 */
typedef struct ompi_free_list_pos_t {
    unsigned char* last_memory;
    unsigned char* last_item;
} ompi_free_list_pos_t;
#define OMPI_FREE_LIST_POS_BEGINING {NULL, NULL}

OMPI_DECLSPEC int ompi_free_list_parse( ompi_free_list_t* list,
                                        ompi_free_list_pos_t* position,
                                        opal_list_item_t** return_item );
/**
 * Attemp to obtain an item from a free list. 
 *
 * @param fl (IN)        Free list.
 * @param item (OUT)     Allocated item.
 * @param rc (OUT)       OMPI_SUCCESS or error status on failure.
 *
 * If the requested item is not available the free list is grown to 
 * accomodate the request - unless the max number of allocations has 
 * been reached.  If this is the case - an out of resource error is 
 * returned to the caller.
 */
 
#define OMPI_FREE_LIST_GET(fl, item, rc) \
{ \
    if(opal_using_threads()) { \
        item = (ompi_free_list_item_t*) opal_atomic_lifo_pop(&((fl)->super)); \
        if(NULL == item) { \
            opal_mutex_lock(&((fl)->fl_lock)); \
            ompi_free_list_grow((fl), (fl)->fl_num_per_alloc); \
            opal_mutex_unlock(&((fl)->fl_lock)); \
            item = (ompi_free_list_item_t*) opal_atomic_lifo_pop(&((fl)->super)); \
        } \
    } else { \
        item = (ompi_free_list_item_t*) opal_atomic_lifo_pop(&((fl)->super)); \
        if(NULL == item) { \
            ompi_free_list_grow((fl), (fl)->fl_num_per_alloc); \
            item =(ompi_free_list_item_t*) opal_atomic_lifo_pop(&((fl)->super)); \
        } \
    }  \
    rc = (NULL == item) ?  OMPI_ERR_TEMP_OUT_OF_RESOURCE : OMPI_SUCCESS; \
} 

/**
 * Blocking call to obtain an item from a free list.
 *
 * @param fl (IN)        Free list.
 * @param item (OUT)     Allocated item.
 * @param rc (OUT)       OMPI_SUCCESS or error status on failure.
 *
 * If the requested item is not available the free list is grown to 
 * accomodate the request - unless the max number of allocations has 
 * been reached. In this case the caller is blocked until an item
 * is returned to the list.
 */

#define OMPI_FREE_LIST_WAIT(fl, item, rc)                                  \
    rc = __ompi_free_list_wait( (fl), &(item) )

static inline int __ompi_free_list_wait( ompi_free_list_t* fl,
                                         ompi_free_list_item_t** item )
{
    *item = (ompi_free_list_item_t*)opal_atomic_lifo_pop(&((fl)->super));
    while( NULL == *item ) {
        if( !OPAL_THREAD_TRYLOCK(&((fl)->fl_lock)) ) {
            if((fl)->fl_max_to_alloc <= (fl)->fl_num_allocated) {
                (fl)->fl_num_waiting++;
                opal_condition_wait(&((fl)->fl_condition), &((fl)->fl_lock));
                (fl)->fl_num_waiting--;
            } else {
                if(ompi_free_list_grow((fl), (fl)->fl_num_per_alloc)
                        == OMPI_SUCCESS) {
                    if( 0 < (fl)->fl_num_waiting ) {
                        if( 1 == (fl)->fl_num_waiting ) {
                            opal_condition_signal(&((fl)->fl_condition));
                        } else {
                            opal_condition_broadcast(&((fl)->fl_condition));
                        }
                    }
                } else {
                    (fl)->fl_num_waiting++;
                    opal_condition_wait(&((fl)->fl_condition), &((fl)->fl_lock));
                    (fl)->fl_num_waiting--;
                }
            }
        } else {
            /* If I wasn't able to get the lock in the begining when I finaly grab it
             * the one holding the lock in the begining already grow the list. I will
             * release the lock and try to get a new element until I succeed.
             */
            OPAL_THREAD_LOCK(&((fl)->fl_lock));
        }
        OPAL_THREAD_UNLOCK(&((fl)->fl_lock));
        *item = (ompi_free_list_item_t*)opal_atomic_lifo_pop(&((fl)->super));
    }
    return OMPI_SUCCESS;
} 

/**
 * Return an item to a free list. 
 *
 * @param fl (IN)        Free list.
 * @param item (OUT)     Allocated item.
 *
 */
 
#define OMPI_FREE_LIST_RETURN(fl, item)                                 \
    do {                                                                \
        opal_list_item_t* original;                                     \
                                                                        \
        original = opal_atomic_lifo_push( &(fl)->super,                 \
                                          &(item)->super);              \
        if( &(fl)->super.opal_lifo_ghost == original ) {                \
            OPAL_THREAD_LOCK(&(fl)->fl_lock);                           \
            if((fl)->fl_num_waiting > 0) {                              \
                if( 1 == (fl)->fl_num_waiting ) {                       \
                    opal_condition_signal(&((fl)->fl_condition));       \
                } else {                                                \
                    opal_condition_broadcast(&((fl)->fl_condition));    \
                }                                                       \
            }                                                           \
            OPAL_THREAD_UNLOCK(&(fl)->fl_lock);                         \
        }                                                               \
    } while(0)
    
#if defined(c_plusplus) || defined(__cplusplus)
}
#endif
#endif 

