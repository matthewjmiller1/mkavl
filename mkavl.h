/**
 * @file
 * @author Matt Miller <matt@matthewjmiller.net>
 *
 * @section LICENSE
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @section DESCRIPTION
 *
 * This is the public interface for the multi-key AVL interface.
 */
#ifndef __MKAVL_H__
#define __MKAVL_H__

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

/** Opaque pointer to reference instances of AVL trees */
typedef struct mkavl_tree_st_ *mkavl_tree_handle;

/** Opaque pointer to reference instances of AVL iterators */
typedef struct mkavl_iterator_st_ *mkavl_iterator_handle;

/**
 * Return codes used to indicate whether a function call was successful.
 */
typedef enum mkavl_rc_e_ {
    /** Invalid return code, should never be used */
    MKAVL_RC_E_INVALID,
    /** Successful return */
    MKAVL_RC_E_SUCCESS,
    /** Function received an invalid input */
    MKAVL_RC_E_EINVAL,
    /** Function failed to allocate memory */
    MKAVL_RC_E_ENOMEM,
    /** Internal data structures are out of sync*/
    MKAVL_RC_E_EOOSYNC,
    /** Max return code for bounds testing */
    MKAVL_RC_E_MAX,
} mkavl_rc_e;

typedef enum mkavl_find_type_e_ {
    MKAVL_FIND_TYPE_E_INVALID,
    MKAVL_FIND_TYPE_E_EQUAL,
    MKAVL_FIND_TYPE_E_GT,
    MKAVL_FIND_TYPE_E_LT,
    MKAVL_FIND_TYPE_E_GE,
    MKAVL_FIND_TYPE_E_LE,
    MKAVL_FIND_TYPE_E_MAX,
} mkavl_find_type_e;

/**
 * Prototype for allocating items.
 */
typedef void *
(*mkavl_malloc_fn)(size_t size);

/**
 * Prototype for freeing items.
 */
typedef void
(*mkavl_free_fn)(void *ptr);

/**
 * Specifies the allocator functions for an AVL tree.
 */
typedef struct mkavl_allocator_st_ {
    /** The allocating function */
    mkavl_malloc_fn malloc_fn;
    /** The freeing function */
    mkavl_free_fn free_fn;
} mkavl_allocator_st;

/**
 * Prototype for comparing two items.  The context is what was passed in when
 * the AVL tree was created.
 *
 * @return Zero if item1 and item2 are equal, 1 if item1 is greater, and -1 if
 * item2 is greater.
 */
typedef int32_t
(*mkavl_compare_fn)(const void *item1, const void *item2,
                    void *context);

/**
 * Prototype for a function that is applied to every element for various
 * operations.
 */
typedef void
(*mkavl_item_fn)(void *item, void *context);

/**
 * Prototype for a function that is applied when copying items from an existing
 * tree to a new tree.
 *
 * @return A pointer to the item to be placed into the new tree.
 */
typedef void *
(*mkavl_copy_fn)(void *item, void *context);

/**
 * Prototype for a function applied to items when walking over the entire tree.
 *
 * @param item The current item on which to operate.
 * @param tree_context The context for the tree.
 * @param walk_context The context for the current walk.
 * @param stop_walk Impelementor should set to true to stop the walk upon
 * return.
 */
typedef mkavl_rc_e
(*mkavl_walk_cb_fn)(void *item, void *tree_context, void *walk_context,
                    bool *stop_walk);

/* APIs below are documented in their implementation file */

/* Utility functions */

extern bool
mkavl_rc_e_is_notok(mkavl_rc_e rc);

extern bool
mkavl_rc_e_is_ok(mkavl_rc_e rc);

extern bool
mkavl_rc_e_is_valid(mkavl_rc_e rc);

extern const char *
mkavl_rc_e_get_string(mkavl_rc_e rc);

extern bool
mkavl_find_type_e_is_valid(mkavl_find_type_e type);

extern const char *
mkavl_find_type_e_get_string(mkavl_find_type_e type);

/* AVL APIs */

extern mkavl_rc_e
mkavl_new(mkavl_tree_handle *tree_h,
          mkavl_compare_fn *compare_fn_array, 
          size_t compare_fn_array_size, 
          void *context, mkavl_allocator_st *allocator);

extern mkavl_rc_e
mkavl_delete(mkavl_tree_handle *tree_h, mkavl_item_fn item_fn);

extern mkavl_rc_e
mkavl_copy(mkavl_tree_handle source_tree_h, mkavl_tree_handle *new_tree_h,
           mkavl_copy_fn copy_fn, mkavl_item_fn item_fn, 
           mkavl_allocator_st *allocator);

extern mkavl_rc_e
mkavl_add(mkavl_tree_handle tree_h, void *item_to_add, 
          void **existing_item);

extern mkavl_rc_e
mkavl_find(mkavl_tree_handle tree_h, mkavl_find_type_e type,
           size_t key_idx, const void *lookup_item, void **found_item);

extern mkavl_rc_e
mkavl_remove(mkavl_tree_handle tree_h, const void *item_to_remove,
             void **found_item);

extern mkavl_rc_e
mkavl_add_key_idx(mkavl_tree_handle tree_h, size_t key_idx,
                  void *item_to_add, void **existing_item);

extern mkavl_rc_e
mkavl_remove_key_idx(mkavl_tree_handle tree_h, size_t key_idx,
                     const void *item_to_remove, void **found_item);

/* AVL utility functions */

extern uint32_t
mkavl_count(mkavl_tree_handle tree_h);

extern mkavl_rc_e
mkavl_walk(mkavl_tree_handle tree_h, mkavl_walk_cb_fn cb_fn,
           void *walk_context);

// TODO: selective delete walk function?

/* AVL iterator functions. */

extern mkavl_rc_e
mkavl_iter_new(mkavl_iterator_handle *iterator_h, mkavl_tree_handle tree_h,
               size_t key_idx);

extern mkavl_rc_e
mkavl_iter_delete(mkavl_iterator_handle *iterator_h);

extern mkavl_rc_e
mkavl_iter_first(mkavl_iterator_handle iterator_h, void **item);

extern mkavl_rc_e
mkavl_iter_last(mkavl_iterator_handle iterator_h, void **item);

extern mkavl_rc_e
mkavl_iter_find(mkavl_iterator_handle iterator_h, void *lookup_item,
                void **found_item);

extern mkavl_rc_e
mkavl_iter_next(mkavl_iterator_handle iterator_h, void **item);

extern mkavl_rc_e
mkavl_iter_prev(mkavl_iterator_handle iterator_h, void **item);

extern mkavl_rc_e
mkavl_iter_cur(mkavl_iterator_handle iterator_h, void **item);

#endif
