/**
 * @file
 * @author Matt Miller <matt@matthewjmiller.net>
 *
 * @section LICENSE
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
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

/**
 * @mainpage mkavl: Multi-Key AVL Trees
 * \author Matthew J. Miller (matt@matthewjmiller.net)
 * \date 2011
 *
 * @section sec_intro Introduction
 *
 * The idea behind multi-key AVL trees is that a single item can be keyed
 * multiple ways to different allow <i>O(lg N)</i> lookups.  The simplest
 * example would be if you have items that have two separate key fields.  For
 * example, if you have an employee database where each employee has a unique ID
 * and phone number.  The multi-key AVL essentially manages two separate AVLs
 * under the covers for the same employee data, one keyed by ID and one by phone
 * number, so an employee can be looked up efficiently by either field.
 *
 * When the mkavl tree is created, \e M different comparison functions are
 * given.  When mkavl_add is called, the item is inserted in \e M different
 * AVLs, with all \e M AVL nodes pointing to the same data item.
 *
 * The only similar data structure I could find was 
 * <a 
 * href="http://www.boost.org/doc/libs/1_46_1/libs/multi_index/doc/index.html">
 * the \c multi_index library</a>
 * in C++'s Boost.
 *
 * The backend AVL implementation comes from 
 * <a href="http://www.stanford.edu/~blp/avl/">Ben Plaff's open source 
 * AVL library</a>.
 *
 * In essence, this adds two things the backend AVL implementation.  First, it
 * more transparently handles a single data item being added to multiple AVL
 * trees (each keyed differently).  Second, it provides for greater than and
 * less than lookups for keys not in the tree.  E.g., if you wanted to find the
 * first ID greater than X, you can do this even if X itself doesn't exist in
 * the AVL.
 *
 * For the unit test of this library, see test_mkavl.c.  For example usage,
 * see employee_example.c and malloc_example.c.
 *
 * \section sec_example Example
 *
 * A more complex example is, say you want to be able to query your employee
 * database to find all employees with a given last name without walking the
 * entire database.  Let's say the employee ID is the primary key.  You could
 * create an mkavl tree with two comparison function keyed in the following
 * manner:
 *
 *    -# <tt>Key1: &lt;ID&gt;</tt>
 *    -# <tt>Key2: <LastName | ID></tt>
 *
 * Knowing the zero is the minimum ID value, you could lookup the first employee
 * with the last name "Smith" in <i>O(lg N)</i> time by doing a greater than or
 * equal to lookup on the key <tt><"Smith" | 0></tt>.  If the record returned
 * doesn't have the the LastName "Smith", then there are no matching records.
 * Otherwise, you can continue iterating through all the "Smith" records doing
 * greater than lookups until you hit NULL or a non-"Smith" record.
 *
 * \section sec_usage Usage
 *
 * Just run <tt>make all</tt> to build the dynamic and shared libraries in lib/.
 * Use "-lmkavl" to link the library.
 * Running <tt>make clean</tt> will remove generated files (e.g., .o).
 *
 * @section sec_license GNU General Public License
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

/**
 * The type of find for lookups.
 */
typedef enum mkavl_find_type_e_ {
    /** Invalid type */
    MKAVL_FIND_TYPE_E_INVALID,
    /** Find an item equal to */
    MKAVL_FIND_TYPE_E_EQUAL,
    /** First valid find type */
    MKAVL_FIND_TYPE_E_FIRST = MKAVL_FIND_TYPE_E_EQUAL,
    /** Find an item greater than */
    MKAVL_FIND_TYPE_E_GT,
    /** Find an item less than */
    MKAVL_FIND_TYPE_E_LT,
    /** Find an item greater than or equal to */
    MKAVL_FIND_TYPE_E_GE,
    /** Find an item less than or equal to */
    MKAVL_FIND_TYPE_E_LE,
    /** Max value for bounds testing */
    MKAVL_FIND_TYPE_E_MAX,
} mkavl_find_type_e;

/**
 * Prototype for allocating items.
 */
typedef void *
(*mkavl_malloc_fn)(size_t size, void *context);

/**
 * Prototype for freeing items.
 */
typedef void
(*mkavl_free_fn)(void *ptr, void *context);

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
 *
 * @param item The item on which to operate
 * @param context The client context
 * @return The return code
 */
typedef mkavl_rc_e
(*mkavl_item_fn)(void *item, void *context);

/**
 * Prototype for a function that is a callback to the client to operate on its
 * context when mkavl_delete() is called.
 *
 * @param context The client context
 * @return The return code
 */
typedef mkavl_rc_e
(*mkavl_delete_context_fn)(void *context);

/**
 * Prototype for a function that is applied when copying items from an existing
 * tree to a new tree.
 *
 * @param item Item to be copied
 * @param context Client context for the tree
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
          size_t compare_fn_array_count, 
          void *context, mkavl_allocator_st *allocator);

extern void *
mkavl_get_tree_context(mkavl_tree_handle tree_h);

extern mkavl_rc_e
mkavl_delete(mkavl_tree_handle *tree_h, mkavl_item_fn item_fn, 
             mkavl_delete_context_fn delete_context_fn);

extern mkavl_rc_e
mkavl_copy(const mkavl_tree_handle source_tree_h, 
           mkavl_tree_handle *new_tree_h,
           mkavl_copy_fn copy_fn, mkavl_item_fn item_fn, 
           bool use_source_context, void *new_context,
           mkavl_delete_context_fn delete_context_fn,
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
