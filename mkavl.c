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
 * This is the implementation for the multi-key AVL interface.
 */

#include "mkavl.h"
#include "libavl/avl.h"

typedef struct mkavl_tree_st_ {
    uint8_t dummy;
} mkavl_tree_st;

typedef struct mkavl_iterator_st_ {
    uint8_t dummy;
} mkavl_iterator_st;

bool
mkavl_rc_e_is_notok (mkavl_rc_e rc)
{
    return (false);
}

bool
mkavl_rc_e_is_ok (mkavl_rc_e rc)
{
    return (false);
}

bool
mkavl_rc_e_is_valid (mkavl_rc_e rc)
{
    return (false);
}

const char *
mkavl_rc_e_get_string (mkavl_rc_e rc)
{
    return ("");
}

bool
mkavl_lookup_type_e_is_valid (mkavl_lookup_type_e type)
{
    return (false);
}

const char *
mkavl_lookup_type_e_get_string (mkavl_lookup_type_e rc)
{
    return ("");

}

mkavl_rc_e
mkavl_new (mkavl_tree_handle *tree_h,
           mkavl_compare_fn *compare_fn_array, 
           size_t compare_fn_array_size, 
           void *context, mkavl_allocator_st *allocator)
{
    return (MKAVL_RC_E_SUCCESS);
}

mkavl_rc_e
mkavl_delete (mkavl_tree_handle *tree_h, mkavl_item_fn item_fn)
{
    return (MKAVL_RC_E_SUCCESS);
}

mkavl_rc_e
mkavl_copy (mkavl_tree_handle source_tree_h, mkavl_tree_handle *new_tree_h,
            mkavl_copy_fn copy_fn, mkavl_item_fn item_fn, 
            mkavl_allocator_st *allocator)
{
    return (MKAVL_RC_E_SUCCESS);
}

mkavl_rc_e
mkavl_add (mkavl_tree_handle tree_h, void *item_to_add, 
           void **existing_item)
{
    return (MKAVL_RC_E_SUCCESS);
}

mkavl_rc_e
mkavl_find (mkavl_tree_handle tree_h, mkavl_find_type_e type,
            size_t key_idx, const void *lookup_item, void **found_item)
{
    return (MKAVL_RC_E_SUCCESS);
}

mkavl_rc_e
mkavl_remove (mkavl_tree_handle tree_h, const void *item_to_remove,
              void **found_item)
{
    return (MKAVL_RC_E_SUCCESS);
}

mkavl_rc_e
mkavl_add_key_idx (mkavl_tree_handle tree_h, size_t key_idx,
                   void *item_to_add, void **existing_item)
{
    return (MKAVL_RC_E_SUCCESS);
}

mkavl_rc_e
mkavl_remove_key_idx (mkavl_tree_handle tree_h, size_t key_idx,
                      const void *item_to_remove, void **found_item)
{
    return (MKAVL_RC_E_SUCCESS);
}

uint32_t
mkavl_count (mkavl_tree_handle tree_h tree_h, size_t key_idx)
{
    return (0);
}

mkavl_rc_e
mkavl_walk (mkavl_tree_handle tree_h tree_h, mkavl_walk_cb_fn cb_fn,
            void *walk_context)
{
    return (MKAVL_RC_E_SUCCESS);
}

mkavl_rc_e
mkavl_iter_new (mkavl_iterator_handle *iterator_h, mkavl_tree_handle tree_h,
                size_t key_idx)
{
    return (MKAVL_RC_E_SUCCESS);
}

mkavl_rc_e
mkavl_iter_delete (mkavl_iterator_handle *iterator_h)
{
    return (MKAVL_RC_E_SUCCESS);
}

mkavl_rc_e
mkavl_iter_first (mkavl_iterator_handle iterator_h, void **item)
{
    return (MKAVL_RC_E_SUCCESS);
}

mkavl_rc_e
mkavl_iter_last (mkavl_iterator_handle iterator_h, void **item)
{
    return (MKAVL_RC_E_SUCCESS);
}

mkavl_rc_e
mkavl_iter_find (mkavl_iterator_handle iterator_h, void *lookup_item,
                 void **found_item)
{
    return (MKAVL_RC_E_SUCCESS);
}

mkavl_rc_e
mkavl_iter_next (mkavl_iterator_handle iterator_h, void **item)
{
    return (MKAVL_RC_E_SUCCESS);
}

mkavl_rc_e
mkavl_iter_prev (mkavl_iterator_handle iterator_h, void **item)
{
    return (MKAVL_RC_E_SUCCESS);
}

mkavl_rc_e
mkavl_iter_cur (mkavl_iterator_handle iterator_h, void **item)
{
    return (MKAVL_RC_E_SUCCESS);
}
