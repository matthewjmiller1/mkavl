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

/**
 * Compile time assert macro from:
 * http://www.pixelbeat.org/programming/gcc/static_assert.html 
 */
#define CT_ASSERT(e) extern char (*CT_ASSERT(void)) [sizeof(char[1 - 2*!(e)])]

/**
 * Determine the number of elements in an array.
 */
#define NELEMS(x) (sizeof(x) / sizeof(x[0]))

/**
 * Magic number indicating a pointer is valid for sanity checks.
 */
#define MKAVL_CTX_MAGIC 0xCAFEBABE

/**
 * Magic number indicating a pointer is stale for sanity checks.
 */
#define MKAVL_CTX_STALE 0xDEADBEEF

typedef struct mkavl_avl_ctx_st_ {
    uint32_t magic;
    mkavl_tree_handle tree;
    size_t key_idx;
} mkavl_avl_ctx_st;

typedef struct mkavl_avl_tree_st_ {
    struct avl_table *tree;
    mkavl_compare_fn compare_fn;
    mkavl_avl_ctx_st *avl_ctx;
} mkavl_avl_tree_st;

typedef struct mkavl_allocator_wrapper_st_ {
    /**
     * The libavl allocator.  Must be the first member for use in casting.
     */
    struct libavl_allocator avl_allocator;
    uint32_t magic;
    mkavl_allocator_st mkavl_allocator;
} mkavl_allocator_wrapper_st;

typedef struct mkavl_tree_st_ {
    void *context;
    mkavl_allocator_wrapper_st allocator;
    size_t avl_tree_count;
    mkavl_avl_tree_st *avl_tree_array;
} mkavl_tree_st;

typedef struct mkavl_iterator_st_ {
    uint8_t dummy;
} mkavl_iterator_st;

/**
 * String representations of the return codes.
 *
 * @see mkavl_rc_e
 */
static const char * const mkavl_rc_e_string[] = {
    "Invalid RC",
    "Success",
    "Invalid input",
    "No memory",
    "Max RC"
};

/** @cond doxygen_suppress */
/* Ensure there is a string for each enum declared */
CT_ASSERT(NELEMS(mkavl_rc_e_string) == (MKAVL_RC_E_MAX + 1));
/** @endcond */

/**
 * Indicates whether the return code is not in error.
 *
 * @param rc The return code to check
 * @return true if the return code is not in error.
 */
bool
mkavl_rc_e_is_notok (mkavl_rc_e rc)
{
    return (MKAVL_RC_E_SUCCESS != rc);
}

/**
 * Indicates whether the return code is in error.
 *
 * @param rc The return code to check
 * @return true if the return code is not in error.
 */
bool
mkavl_rc_e_is_ok (mkavl_rc_e rc)
{
    return (MKAVL_RC_E_SUCCESS == rc);
}

/**
 * Indicates whether the return code is valid.
 *
 * @param rc The return code to check
 * @return true if the return code is valid.
 */
bool
mkavl_rc_e_is_valid (mkavl_rc_e rc)
{
    return ((rc >= MKAVL_RC_E_INVALID) && (rc <= MKAVL_RC_E_MAX));
}

/**
 * Get a string representation of the return code.
 *
 * @param rc The return code
 * @return A string representation of the return code or "__Invalid__" if an
 * invalid return code is input.
 */
const char *
mkavl_rc_e_get_string (mkavl_rc_e rc)
{
    const char* retval = "__Invalid__";

    if (mkavl_rc_e_is_valid(rc)) {
        retval = mkavl_rc_e_string[rc];
    }

    return (retval);
}

/**
 * String representations of the return codes.
 *
 * @see mkavl_find_type_e
 */
static const char * const mkavl_find_type_e_string[] = {
    "Invalid",
    "Equal",
    "Greater than",
    "Less than",
    "Greater than or equal",
    "Less than or equal",
    "Max type"
};

/** @cond doxygen_suppress */
/* Ensure there is a string for each enum declared */
CT_ASSERT(NELEMS(mkavl_find_type_e_string) == (MKAVL_FIND_TYPE_E_MAX + 1));
/** @endcond */

/**
 * Indicates whether the return code is valid.
 *
 * @param rc The return code to check
 * @return true if the return code is valid.
 */
bool
mkavl_find_type_e_is_valid (mkavl_find_type_e type)
{
    return ((type >= MKAVL_FIND_TYPE_E_INVALID) && 
            (type <= MKAVL_FIND_TYPE_E_MAX));
}

/**
 * Get a string representation of the find type.
 *
 * @param type The find type
 * @return A string representation of the return code or "__Invalid__" if an
 * invalid return code is input.
 */
const char *
mkavl_find_type_e_get_string (mkavl_find_type_e type)
{
    const char* retval = "__Invalid__";

    if (mkavl_find_type_e_is_valid(type)) {
        retval = mkavl_find_type_e_string[type];
    }

    return (retval);
}

static void *
mkavl_malloc_wrapper (struct libavl_allocator *allocator, size_t size)
{
    mkavl_allocator_wrapper_st *mkavl_allocator =
        (mkavl_allocator_wrapper_st *) allocator;

    if ((NULL == mkavl_allocator) ||
        (MKAVL_CTX_MAGIC != mkavl_allocator->magic) ||
        (NULL == mkavl_allocator->mkavl_allocator.malloc_fn)) {
        return (NULL);
    }

    return (mkavl_allocator->mkavl_allocator.malloc_fn(size));
}

static void
mkavl_free_wrapper (struct libavl_allocator *allocator, void *libavl_block)
{
    mkavl_allocator_wrapper_st *mkavl_allocator =
        (mkavl_allocator_wrapper_st *) allocator;

    if ((NULL == mkavl_allocator) ||
        (MKAVL_CTX_MAGIC != mkavl_allocator->magic) ||
        (NULL == mkavl_allocator->mkavl_allocator.free_fn)) {
        return;
    }

    return (mkavl_allocator->mkavl_allocator.free_fn(libavl_block));
}

static mkavl_allocator_st mkavl_allocator_default = {
    malloc,
    free
};

static struct libavl_allocator mkavl_allocator_wrapper = {
    mkavl_malloc_wrapper,
    mkavl_free_wrapper
};

static int
mkavl_compare_wrapper (const void *avl_a, const void *avl_b, void *avl_param)
{
    mkavl_avl_ctx_st *avl_ctx;
    mkavl_compare_fn cmp_fn;

    avl_ctx = (mkavl_avl_ctx_st *) avl_param;
    if ((NULL == avl_ctx) || (MKAVL_CTX_MAGIC != avl_ctx->magic) ||
        (NULL == avl_ctx->tree) || 
        (avl_ctx->key_idx >= avl_ctx->tree->avl_tree_count)) {
        // TODO
    }

    cmp_fn = avl_ctx->tree->avl_tree_array[avl_ctx->key_idx].compare_fn;

    return (cmp_fn(avl_a, avl_b, avl_ctx->tree->context));
}

static mkavl_rc_e
mkavl_delete_tree (mkavl_tree_st **tree)
{
    mkavl_allocator_st local_allocator;
    mkavl_tree_st *local_tree;
    uint32_t i;

    if (NULL == tree) {
        return (MKAVL_RC_E_EINVAL);
    }
    local_tree = *tree;

    memcpy(&local_allocator, &(local_tree->allocator.mkavl_allocator),
           sizeof(local_allocator));

    if (NULL == local_tree) {
        return (MKAVL_RC_E_SUCCESS);
    }

    local_tree->allocator.magic = MKAVL_CTX_STALE;
    if (NULL != local_tree->avl_tree_array) {
        for (i = 0; i < local_tree->avl_tree_count; ++i) {
            if (NULL != local_tree->avl_tree_array[i].tree) { 
                if (NULL != local_tree->avl_tree_array[i].avl_ctx) {
                    local_allocator.free_fn(
                        local_tree->avl_tree_array[i].avl_ctx);
                }
                avl_destroy(local_tree->avl_tree_array[i].tree, NULL);
            }
        }
        local_allocator.free_fn(local_tree->avl_tree_array);
    }

    local_allocator.free_fn(local_tree);

    *tree = NULL;

    return (MKAVL_RC_E_SUCCESS);
}

mkavl_rc_e
mkavl_new (mkavl_tree_handle *tree_h,
           mkavl_compare_fn *compare_fn_array, 
           size_t compare_fn_array_size, 
           void *context, mkavl_allocator_st *allocator)
{
    mkavl_allocator_st *local_allocator;
    mkavl_tree_st *new_tree;
    mkavl_avl_ctx_st *avl_ctx;
    mkavl_rc_e rc = MKAVL_RC_E_SUCCESS, err_rc;
    uint32_t i;

    if ((NULL == tree_h) || (NULL == compare_fn_array) ||
        (0 == compare_fn_array_size)) {
        return (MKAVL_RC_E_EINVAL);
    }
    *tree_h = NULL;

    local_allocator = 
        (NULL == allocator) ? &mkavl_allocator_default : allocator;

    new_tree = local_allocator->malloc_fn(sizeof(*new_tree));
    if (NULL == new_tree) {
        rc = MKAVL_RC_E_ENOMEM;
        goto err_exit;
    }

    new_tree->context = context;
    memcpy(&(new_tree->allocator.avl_allocator), &mkavl_allocator_wrapper,
           sizeof(new_tree->allocator.avl_allocator));
    memcpy(&(new_tree->allocator.mkavl_allocator), &local_allocator,
           sizeof(new_tree->allocator));
    new_tree->allocator.magic = MKAVL_CTX_MAGIC;
    new_tree->avl_tree_count = compare_fn_array_size;
    new_tree->avl_tree_array = NULL;

    new_tree->avl_tree_array = 
        local_allocator->malloc_fn(new_tree->avl_tree_count * 
                                   sizeof(*(new_tree->avl_tree_array)));
    if (NULL == new_tree->avl_tree_array) {
        rc = MKAVL_RC_E_ENOMEM;
        goto err_exit;
    }

    for (i = 0; i < new_tree->avl_tree_count; ++i) {
        new_tree->avl_tree_array[i].tree = NULL;
        new_tree->avl_tree_array[i].compare_fn = compare_fn_array[i];
        new_tree->avl_tree_array[i].avl_ctx = NULL;
    }

    for (i = 0; i < new_tree->avl_tree_count; ++i) {
        avl_ctx = local_allocator->malloc_fn(sizeof(*avl_ctx));
        if (NULL == avl_ctx) {
            rc = MKAVL_RC_E_ENOMEM;
            goto err_exit;
        }
        avl_ctx->magic = MKAVL_CTX_STALE;

        new_tree->avl_tree_array[i].tree = 
            avl_create(mkavl_compare_wrapper, avl_ctx,
                       &(new_tree->allocator.avl_allocator));
        if (NULL == new_tree->avl_tree_array[i].tree) {
            rc = MKAVL_RC_E_ENOMEM;
            goto err_exit;
        }

        avl_ctx->tree = new_tree;
        avl_ctx->key_idx = i;
        avl_ctx->magic = MKAVL_CTX_MAGIC;
        new_tree->avl_tree_array[i].avl_ctx = avl_ctx;
        avl_ctx = NULL;
    }

    *tree_h = new_tree;

    return (rc);

err_exit:

    if (NULL != new_tree) {
        err_rc = mkavl_delete_tree(&new_tree);
        assert(mkavl_rc_e_is_ok(err_rc));
    }

    if (NULL != avl_ctx) {
        /* Ownership of this memory hasn't been transferred to a tree yet */
        local_allocator->free_fn(avl_ctx);
    }

    return (rc);
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
mkavl_count (mkavl_tree_handle tree_h, size_t key_idx)
{
    return (0);
}

mkavl_rc_e
mkavl_walk (mkavl_tree_handle tree_h, mkavl_walk_cb_fn cb_fn,
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
