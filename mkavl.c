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
#ifndef CT_ASSERT
#define CT_ASSERT(e) extern char (*CT_ASSERT(void)) [sizeof(char[1 - 2*!(e)])]
#endif

/**
 * Determine the number of elements in an array.
 */
#ifndef NELEMS
#define NELEMS(x) (sizeof(x) / sizeof(x[0]))
#endif

/**
 * Sanity value to make sure we don't get stuck in an infinite loop.
 */
#define MKAVL_RUNAWAY_SANITY 100000

/**
 * Magic number indicating a pointer is valid for sanity checks.
 */
#define MKAVL_CTX_MAGIC 0xCAFEBABE

/**
 * Magic number indicating a pointer is stale for sanity checks.
 */
#define MKAVL_CTX_STALE 0xDEADBEEF

/**
 * The internal context data for AVL callbacks.
 */
typedef struct mkavl_avl_ctx_st_ {
    /** Used for sanity checks */
    uint32_t magic;
    /** The mkavl tree associated with the callback */
    mkavl_tree_handle tree_h;
    /** The index in the mkavl tree of the AVL tree for the callback */
    size_t key_idx;
} mkavl_avl_ctx_st;

/**
 * Maintains info on the AVL data associated with an AVL tree in the mkavl tree.
 */
typedef struct mkavl_avl_tree_st_ {
    /** The AVL tree pointer */
    struct avl_table *tree;
    /** The comparison function to use for the tree */
    mkavl_compare_fn compare_fn;
    /** The context used for the AVL tree */
    mkavl_avl_ctx_st *avl_ctx;
} mkavl_avl_tree_st;

/**
 * A wrapper structure to map the mkavl_allocator to the avl_allocator.
 */
typedef struct mkavl_allocator_wrapper_st_ {
    /**
     * The libavl allocator.  Must be the first member for use in casting.
     */
    struct libavl_allocator avl_allocator;
    /** Used for sanity checks */
    uint32_t magic;
    /** The mkavl allocator */
    mkavl_allocator_st mkavl_allocator;
} mkavl_allocator_wrapper_st;

/**
 * The internal representation of the mkavl tree object.
 */
typedef struct mkavl_tree_st_ {
    /** The opaque context passed in by the client */
    void *context;
    /** The memory allocator info passed in by the client */
    mkavl_allocator_wrapper_st allocator;
    /** The number of AVL trees within this object */
    size_t avl_tree_count;
    /** An array of the AVL tree info of size avl_tree_count */
    mkavl_avl_tree_st *avl_tree_array;
    /** 
     * A count of the number of data items inserted by the user.  This is not
     * the total over all AVL trees.  E.g., if there are 3 AVL trees and 5 items
     * have been inserted in the mkavl, then this value is 5, not 15.
     */
    uint32_t item_count;
    /** 
     * The copy function passed in my the client to apply to items when
     * mkavl_copy is done.
     */
    mkavl_copy_fn copy_fn;
} mkavl_tree_st;

/**
 * The internal representation of the mkavl iterator.
 */
typedef struct mkavl_iterator_st_ {
    /** The AVL traverser to use */
    struct avl_traverser avl_t;
    /** The mkavl tree associaed with the iteration */
    mkavl_tree_handle tree_h;
    /** The index in the mkavl tree of the AVL tree for the iteration */
    size_t key_idx;
} mkavl_iterator_st;

/**
 * Assert utility to crash (via abort()) if the condition is not met regardless
 * of whether NDEBUG is defined.  E.g., if we hit an error condition where the
 * data structures are irreversibly out of sync.
 *
 * @param condition The condition for which a crash will happen if false.
 */
static inline void
mkavl_assert_abort (bool condition)
{
    if (!condition) {
        abort();
    }
}

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
    "Out of sync",
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
 * @param type The type to check
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

/**
 * Sanity check for mkavl_avl_ctx_st objects.
 *
 * @param avl_ctx The object to check.  If NULL, false is returned.
 * @return true if the object is valid.
 */
static bool
mkavl_avl_ctx_is_valid (mkavl_avl_ctx_st *avl_ctx)
{
    bool is_valid = true;

    if (is_valid && (NULL == avl_ctx)) {
        is_valid = false;
    }

    if (is_valid && (MKAVL_CTX_MAGIC != avl_ctx->magic)) {
        is_valid = false;
    }

    if (is_valid && (NULL == avl_ctx->tree_h)) {
        is_valid = false;
    }

    if (is_valid && (avl_ctx->key_idx >= avl_ctx->tree_h->avl_tree_count)) {
        is_valid = false;
    }

    return (is_valid);
}

/**
 * Sanity check for mkavl_allocator_wrapper_st objects.
 *
 * @param allocator The object to check.  If NULL, false is returned.
 * @return true if the object is valid.
 */
static bool
mkavl_allocator_wrapper_is_valid (mkavl_allocator_wrapper_st *allocator)
{
    bool is_valid = true;

    if (is_valid && (NULL == allocator)) {
        is_valid = false;
    }

    if (is_valid && (MKAVL_CTX_MAGIC != allocator->magic)) {
        is_valid = false;
    }

    if (is_valid && (NULL == allocator->mkavl_allocator.malloc_fn)) {
        is_valid = false;
    }

    if (is_valid && (NULL == allocator->mkavl_allocator.free_fn)) {
        is_valid = false;
    }

    return (is_valid);
}

/**
 * A wrapper to map the AVL callback to the client callback for the mkavl tree.
 *
 * @param allocator The memory allocator associated with the callback
 * @param size The size to allocate
 * @return A pointer to the new memory, or NULL on failure.
 */
static void *
mkavl_malloc_wrapper (struct libavl_allocator *allocator, size_t size)
{
    mkavl_allocator_wrapper_st *mkavl_allocator =
        (mkavl_allocator_wrapper_st *) allocator;

    if (!mkavl_allocator_wrapper_is_valid(mkavl_allocator)) {
        return (NULL);
    }

    return (mkavl_allocator->mkavl_allocator.malloc_fn(size));
}

/**
 * A wrapper to map the AVL callback to the client callback for the mkavl tree.
 *
 * @param allocator The memory allocator associated with the callback
 * @param libavl_block The memory to free
 */
static void
mkavl_free_wrapper (struct libavl_allocator *allocator, void *libavl_block)
{
    mkavl_allocator_wrapper_st *mkavl_allocator =
        (mkavl_allocator_wrapper_st *) allocator;

    if (!mkavl_allocator_wrapper_is_valid(mkavl_allocator)) {
        return;
    }

    return (mkavl_allocator->mkavl_allocator.free_fn(libavl_block));
}

/* By default, we'll just use malloc and free if the client passes nothing in */
static mkavl_allocator_st mkavl_allocator_default = {
    malloc,
    free
};

static struct libavl_allocator mkavl_allocator_wrapper = {
    mkavl_malloc_wrapper,
    mkavl_free_wrapper
};

/**
 * A wrapper to map the AVL callback to the client callback for the mkavl tree.
 *
 * @param avl_a The first item in the comparison
 * @param avl_b The second item in the comparison
 * @param avl_param The context associated with the callback
 * @return 0 if equal, -1 if avl_a < avl_b, and 1 if avl_a > avl_b
 */
static int
mkavl_compare_wrapper (const void *avl_a, const void *avl_b, void *avl_param)
{
    mkavl_avl_ctx_st *avl_ctx;
    mkavl_compare_fn cmp_fn;

    avl_ctx = (mkavl_avl_ctx_st *) avl_param;
    mkavl_assert_abort(mkavl_avl_ctx_is_valid(avl_ctx));

    cmp_fn = avl_ctx->tree_h->avl_tree_array[avl_ctx->key_idx].compare_fn;
    mkavl_assert_abort(NULL != cmp_fn);

    return (cmp_fn(avl_a, avl_b, avl_ctx->tree_h->context));
}

/**
 * A wrapper to map the AVL callback to the client callback for the mkavl tree.
 *
 * @param avl_item The AVL item to copy
 * @param avl_param The context associated with the callback
 * @return The new, copied item
 */
static void *
mkavl_copy_wrapper (void *avl_item, void *avl_param)
{
    mkavl_avl_ctx_st *avl_ctx;
    mkavl_copy_fn copy_fn;

    avl_ctx = (mkavl_avl_ctx_st *) avl_param;
    mkavl_assert_abort(mkavl_avl_ctx_is_valid(avl_ctx));

    copy_fn = avl_ctx->tree_h->copy_fn;
    mkavl_assert_abort(NULL != copy_fn);

    return (copy_fn(avl_item, avl_ctx->tree_h->context));
}

/**
 * This will free all memory associated with the tree and set the pointer to
 * NULL.
 *
 * @param tree_h Pointer to the tree to delete
 * @return The return code
 */
static mkavl_rc_e
mkavl_delete_tree (mkavl_tree_handle *tree_h)
{
    mkavl_allocator_st local_allocator;
    mkavl_tree_handle local_tree_h;
    uint32_t i;

    if (NULL == tree_h) {
        return (MKAVL_RC_E_EINVAL);
    }
    local_tree_h = *tree_h;

    memcpy(&local_allocator, &(local_tree_h->allocator.mkavl_allocator),
           sizeof(local_allocator));

    if (NULL == local_tree_h) {
        return (MKAVL_RC_E_SUCCESS);
    }

    local_tree_h->allocator.magic = MKAVL_CTX_STALE;
    if (NULL != local_tree_h->avl_tree_array) {
        for (i = 0; i < local_tree_h->avl_tree_count; ++i) {
            if (NULL != local_tree_h->avl_tree_array[i].tree) { 
                if (NULL != local_tree_h->avl_tree_array[i].avl_ctx) {
                    local_allocator.free_fn(
                        local_tree_h->avl_tree_array[i].avl_ctx);
                }
                avl_destroy(local_tree_h->avl_tree_array[i].tree, NULL);
            }
        }
        local_allocator.free_fn(local_tree_h->avl_tree_array);
    }

    local_allocator.free_fn(local_tree_h);

    *tree_h = NULL;

    return (MKAVL_RC_E_SUCCESS);
}

/**
 * Sanity check for mkavl tree objects.
 *
 * @param tree_h The object to check.  If NULL, false is returned.
 * @return true if the object is valid.
 */
static bool
mkavl_tree_is_valid (mkavl_tree_handle tree_h)
{
    bool is_valid = true;
    uint32_t i;

    if (is_valid && (NULL == tree_h)) {
        is_valid = false;
    }

    if (is_valid && (0 == tree_h->avl_tree_count)) {
        is_valid = false;
    }

    if (is_valid && 
        !mkavl_allocator_wrapper_is_valid(&(tree_h->allocator))) {
        is_valid = false;
    }

    if (is_valid) {
        for (i = 0; i < tree_h->avl_tree_count; ++i) {
            if (NULL == tree_h->avl_tree_array[i].tree) {
                is_valid = false;
                break;
            }

            if (NULL == tree_h->avl_tree_array[i].compare_fn) {
                is_valid = false;
                break;
            }

            if (!mkavl_avl_ctx_is_valid(tree_h->avl_tree_array[i].avl_ctx)) {
                is_valid = false;
                break;
            }
        }
    }

    return (is_valid);
}

/**
 * Sanity check for mkavl_iterator_handle objects.
 *
 * @param iterator_h The object to check.  If NULL, false is returned.
 * @return true if the object is valid.
 */
static bool
mkavl_iterator_is_valid (mkavl_iterator_handle iterator_h)
{
    bool is_valid = true;

    if (is_valid && (NULL == iterator_h)) {
        is_valid = false;
    }

    if (is_valid && (!mkavl_tree_is_valid(iterator_h->tree_h))) {
        is_valid = false;
    }

    if (is_valid && 
        (iterator_h->key_idx >= iterator_h->tree_h->avl_tree_count)) {
        is_valid = false;
    }

    return (is_valid);
}

/**
 * Create a new mkavl tree composed of AVL trees using the given array of
 * comparison functions.
 *
 * @see mkavl_delete
 * @param tree_h A pointer to the memory location for the new tree.
 * @param compare_fn_array An array of size compare_fn_array_count of the
 * comparison functions for the mkavl.  This allows the same set of data items
 * to be stored in different orders by mulitple keys.  A deep copy of the
 * functions is made for the tree_h.  There must be at least one function passed
 * in.
 * @param compare_fn_array_count The size of the compare_fn_array.
 * @param context An opaque context passed back to the client in callbacks.
 * @param allocator The memory allocation functions to use for the tree, or NULL
 * if the default functions are to be used.
 * @return The return value
 */
mkavl_rc_e
mkavl_new (mkavl_tree_handle *tree_h,
           mkavl_compare_fn *compare_fn_array, 
           size_t compare_fn_array_count, 
           void *context, mkavl_allocator_st *allocator)
{
    mkavl_allocator_st *local_allocator;
    mkavl_tree_handle local_tree_h;
    mkavl_avl_ctx_st *avl_ctx;
    mkavl_rc_e rc = MKAVL_RC_E_SUCCESS, err_rc;
    uint32_t i;

    if ((NULL == tree_h) || (NULL == compare_fn_array) ||
        (0 == compare_fn_array_count)) {
        return (MKAVL_RC_E_EINVAL);
    }
    *tree_h = NULL;

    local_allocator = 
        (NULL == allocator) ? &mkavl_allocator_default : allocator;

    local_tree_h = local_allocator->malloc_fn(sizeof(*local_tree_h));
    if (NULL == local_tree_h) {
        rc = MKAVL_RC_E_ENOMEM;
        goto err_exit;
    }

    local_tree_h->context = context;
    memcpy(&(local_tree_h->allocator.avl_allocator), &mkavl_allocator_wrapper,
           sizeof(local_tree_h->allocator.avl_allocator));
    memcpy(&(local_tree_h->allocator.mkavl_allocator), local_allocator,
           sizeof(local_tree_h->allocator));
    local_tree_h->allocator.magic = MKAVL_CTX_MAGIC;
    local_tree_h->avl_tree_count = compare_fn_array_count;
    local_tree_h->avl_tree_array = NULL;
    local_tree_h->item_count = 0;
    local_tree_h->copy_fn = NULL;

    local_tree_h->avl_tree_array = 
        local_allocator->malloc_fn(local_tree_h->avl_tree_count * 
                                   sizeof(*(local_tree_h->avl_tree_array)));
    if (NULL == local_tree_h->avl_tree_array) {
        rc = MKAVL_RC_E_ENOMEM;
        goto err_exit;
    }

    for (i = 0; i < local_tree_h->avl_tree_count; ++i) {
        local_tree_h->avl_tree_array[i].tree = NULL;
        local_tree_h->avl_tree_array[i].compare_fn = compare_fn_array[i];
        if (NULL == local_tree_h->avl_tree_array[i].compare_fn) {
            rc = MKAVL_RC_E_ENOMEM;
            goto err_exit;
        }
        local_tree_h->avl_tree_array[i].avl_ctx = NULL;
    }

    for (i = 0; i < local_tree_h->avl_tree_count; ++i) {
        avl_ctx = local_allocator->malloc_fn(sizeof(*avl_ctx));
        if (NULL == avl_ctx) {
            rc = MKAVL_RC_E_ENOMEM;
            goto err_exit;
        }
        avl_ctx->magic = MKAVL_CTX_STALE;

        local_tree_h->avl_tree_array[i].tree = 
            avl_create(mkavl_compare_wrapper, avl_ctx,
                       &(local_tree_h->allocator.avl_allocator));
        if (NULL == local_tree_h->avl_tree_array[i].tree) {
            rc = MKAVL_RC_E_ENOMEM;
            goto err_exit;
        }

        avl_ctx->tree_h = local_tree_h;
        avl_ctx->key_idx = i;
        avl_ctx->magic = MKAVL_CTX_MAGIC;
        local_tree_h->avl_tree_array[i].avl_ctx = avl_ctx;
        avl_ctx = NULL;
    }

    if (!mkavl_tree_is_valid(local_tree_h)) {
        rc = MKAVL_RC_E_EINVAL;
        goto err_exit;
    }

    *tree_h = local_tree_h;

    return (rc);

err_exit:

    if (NULL != local_tree_h) {
        err_rc = mkavl_delete_tree(&local_tree_h);
        mkavl_assert_abort(mkavl_rc_e_is_ok(err_rc));
    }

    if (NULL != avl_ctx) {
        /* Ownership of this memory hasn't been transferred to a tree yet */
        local_allocator->free_fn(avl_ctx);
    }

    return (rc);
}

/**
 * The destroys the tree that was allocated by mkavl_new.  Note that this
 * doesn't actually free the data of the items as that is left to the client.
 * The item_fn parameter can be used for this purpose.  Upon return, the tree_h
 * memory is set to NULL.  This is O(N lg N) for N items.
 *
 * @see mkavl_new
 * @see mkavl_delete_tree
 * @param tree_h A pointer the the tree to free.
 * @param item_fn This function is applied to each item after it has been
 * removed from all the AVL trees.  This is only called once per item regardless
 * of how many AVL trees there are.  E.g., this could be the function to free
 * the memory for the data.  If NULL, no function is applied.
 * @param delete_context_fn This function is applied to the tree's opaque client
 * context that was given via mkavl_new().  E.g., this could free the client
 * context if it was a heap memory pointer.  If NULL, no function is applied.
 * If given, the function will be called even if the client context is NULL.
 * This function is called after all the items have been deleted and item
 * functions called.
 * @return The return code
 */
mkavl_rc_e
mkavl_delete (mkavl_tree_handle *tree_h, mkavl_item_fn item_fn,
              mkavl_delete_context_fn delete_context_fn)
{
    mkavl_tree_handle local_tree_h;
    void *item, *item_to_delete;
    uint32_t i, first_tree_idx;
    uint32_t runaway_counter = 0;
    struct avl_traverser avl_t = {0};
    mkavl_rc_e rc = MKAVL_RC_E_SUCCESS;
    mkavl_rc_e retval = MKAVL_RC_E_SUCCESS;

    if (NULL == tree_h) {
        return (MKAVL_RC_E_EINVAL);
    }
    local_tree_h = *tree_h;

    if (!mkavl_tree_is_valid(local_tree_h)) {
        return (MKAVL_RC_E_EINVAL);
    }

    /* 
     * Just randomly select the first tree as the one on which to iterate.
     * All trees should have the same set of data, just in different orders.
     */
    first_tree_idx = local_tree_h->avl_tree_count;
    for (i = 0; i < local_tree_h->avl_tree_count; ++i) {
        if (NULL != local_tree_h->avl_tree_array[i].tree) {
            first_tree_idx = i;
            avl_t_init(&avl_t, 
                       local_tree_h->avl_tree_array[first_tree_idx].tree);
            break;
        }
    }

    if (first_tree_idx < local_tree_h->avl_tree_count) {
        /* 
         * May be more efficient to just pull the root out each time instead
         * of traversing to get the first.
         */
        item_to_delete = 
            avl_t_first(&avl_t,
                        local_tree_h->avl_tree_array[first_tree_idx].tree);
        while (NULL != item_to_delete) {
            mkavl_assert_abort(runaway_counter <= MKAVL_RUNAWAY_SANITY);

            for (i = first_tree_idx; i < local_tree_h->avl_tree_count; ++i) {
                if (NULL != local_tree_h->avl_tree_array[i].tree) {
                    item = avl_delete(local_tree_h->avl_tree_array[i].tree,
                                      item_to_delete);
                    mkavl_assert_abort(NULL != item);
                }
            }

            if (NULL != item_fn) {
                rc = item_fn(item_to_delete, local_tree_h->context);
                if (mkavl_rc_e_is_notok(rc)) {
                    retval = rc;
                }
            }

            --(local_tree_h->item_count);

            /* We deleted the last item, so there should be a new first */
            item_to_delete = 
                avl_t_first(&avl_t, 
                            local_tree_h->avl_tree_array[first_tree_idx].tree);
            ++runaway_counter;
        }
    }

    if (NULL != delete_context_fn) {
        rc = delete_context_fn(local_tree_h->context);
        if (mkavl_rc_e_is_notok(rc)) {
            retval = rc;
        }
    }

    rc = mkavl_delete_tree(tree_h);
    mkavl_assert_abort(mkavl_rc_e_is_ok(rc));

    return (retval);
}

/**
 * Deep copy a mkavl tree into a new tree.
 *
 * @param source_tree_h The tree from which to copy.
 * @param new_tree_h A pointer to the new tree to which the copy will be done.
 * @param copy_fn A function that is applied to each item in the source tree
 * before it is copied to the new tree.  This is applied once per item
 * regardless of how many AVL trees there are.  E.g., this may allocate a deep
 * copy of the data.  If NULL, then a shallow copy of the data is copied to the
 * new tree.  Note that this function is called with the source tree's context
 * value.
 * @param item_fn If there is an error copying the new tree, this function is
 * applied to all items in the new tree as it is destroyed.  E.g., this may be a
 * function to free the data.
 * @param use_source_context If true, the client context from source_tree_h is
 * used for the new tree.  If false, new_context is used for the new tree.
 * @param new_context The opaque client context to use for the new tree if
 * user_source_context if false.
 * @param delete_context_fn Upon an error, this function will be applied to the
 * new_context if use_source_context is false.  If use_source_context is true,
 * this is a no-op in the case of an error (will not call on the source tree's
 * context).  If NULL, no function is applied.  If given, the function will be
 * called even if the client context is NULL.
 * @param allocator The memory allocation functions to use for the new tree.
 * @return The return code
 */
mkavl_rc_e
mkavl_copy (mkavl_tree_handle source_tree_h, 
            mkavl_tree_handle *new_tree_h,
            mkavl_copy_fn copy_fn, mkavl_item_fn item_fn, 
            bool use_source_context, void *new_context,
            mkavl_delete_context_fn delete_context_fn,
            mkavl_allocator_st *allocator)
{
    mkavl_tree_handle local_tree_h = NULL;
    mkavl_allocator_st *local_allocator = allocator;
    bool allocated_mkavl_tree = false;
    struct avl_traverser avl_t = {0};
    mkavl_copy_fn old_copy_fn, copy_fn_to_use;
    void *item, *existing_item;
    void *context_to_use;
    mkavl_delete_context_fn delete_context_fn_to_use;
    uint32_t i;
    mkavl_rc_e rc = MKAVL_RC_E_SUCCESS;

    if (NULL == new_tree_h) {
        return (MKAVL_RC_E_EINVAL);
    }
    *new_tree_h = NULL;

    if (!mkavl_tree_is_valid(source_tree_h)) {
        return (MKAVL_RC_E_EINVAL);
    }

    if (NULL == local_allocator) {
        local_allocator = &(source_tree_h->allocator.mkavl_allocator);
    }

    {
        mkavl_compare_fn cmp_fn_array[source_tree_h->avl_tree_count];

        for (i = 0; i < NELEMS(cmp_fn_array); ++i) {
            cmp_fn_array[i] = source_tree_h->avl_tree_array[i].compare_fn;
        }

        context_to_use = new_context;
        if (use_source_context) {
            context_to_use = source_tree_h->context;
        }

        rc = mkavl_new(&local_tree_h,
                       cmp_fn_array, NELEMS(cmp_fn_array),
                       context_to_use, local_allocator);
        if (mkavl_rc_e_is_notok(rc)) {
            goto err_exit;
        }
        allocated_mkavl_tree = true;

        /*
         * Copy the first AVL tree (which applies the user's copy function to
         * each item).  Then, iterate over the new, copied tree and add each
         * item to each of the other AVL trees.  Need to temorarily set the
         * source tree's copy function to the user provided one during this
         * iteration, since mkavl_copy_wrapper will look there for the function
         * to use for the real copy function.
         */
        old_copy_fn = source_tree_h->copy_fn;
        source_tree_h->copy_fn = copy_fn;
        copy_fn_to_use = NULL;
        if (NULL != copy_fn) {
            copy_fn_to_use = mkavl_copy_wrapper;
        }
        local_tree_h->avl_tree_array[0].tree = 
            avl_copy(source_tree_h->avl_tree_array[0].tree, copy_fn_to_use,
                     NULL, &(local_tree_h->allocator.avl_allocator));
        source_tree_h->copy_fn = old_copy_fn;
        if (NULL == local_tree_h->avl_tree_array[0].tree) {
            goto err_exit;
        }

        local_tree_h->item_count = 
            avl_count(local_tree_h->avl_tree_array[0].tree);

        if (local_tree_h->avl_tree_count > 1) {
            item = avl_t_first(&avl_t, local_tree_h->avl_tree_array[0].tree);
            while (NULL != item) {
                for (i = 1; i < local_tree_h->avl_tree_count; ++i) {
                    rc = mkavl_add_key_idx(local_tree_h, i,
                                           item, &existing_item);
                    if (mkavl_rc_e_is_notok(rc)) {
                        goto err_exit;
                    }
                }
                item = avl_t_next(&avl_t);
            }
        }
    }

    *new_tree_h = local_tree_h;

    return (MKAVL_RC_E_SUCCESS);

err_exit:

    if (allocated_mkavl_tree) {
        delete_context_fn_to_use = NULL;
        if (!use_source_context) {
            delete_context_fn_to_use = delete_context_fn;
        }
        /* Just deleting the tree should clean everything up. */
        mkavl_delete(&local_tree_h, item_fn, delete_context_fn_to_use);
    }

    return (rc);
}

/**
 * Add an item to each AVL tree in the mkavl.
 *
 * @param tree_h The mkavl tree.
 * @param item_to_add A pointer to the data to add.
 * @param existing_item If an existing item is found, it is returned.
 * Otherwise, NULL if the item was not found.
 * @return The return code
 */
mkavl_rc_e
mkavl_add (mkavl_tree_handle tree_h, void *item_to_add, 
           void **existing_item)
{
    uint32_t i, err_idx = 0;
    void *item, *first_item = NULL;
    mkavl_rc_e rc = MKAVL_RC_E_SUCCESS;

    if ((NULL == item_to_add) || (NULL == existing_item)) {
        return (MKAVL_RC_E_EINVAL);
    }
    *existing_item = NULL;

    if (!mkavl_tree_is_valid(tree_h)) {
        return (MKAVL_RC_E_EINVAL);
    }

    for (i = 0; i < tree_h->avl_tree_count; ++i) {
        item = avl_insert(tree_h->avl_tree_array[i].tree, item_to_add);
        if (0 == i) {
            first_item = item;
        } else if (first_item != item) {
            err_idx = i + 1;
            rc = MKAVL_RC_E_EOOSYNC;
            goto err_exit;
        }
    }

    if (NULL == first_item) {
        ++(tree_h->item_count);
    }

    *existing_item = first_item;

    return (rc);

err_exit:

    for (i = 0; i < err_idx; ++i) {
        if (NULL == first_item) {
            /* Attempt to remove all the items we added */
            item = avl_delete(tree_h->avl_tree_array[i].tree, item_to_add);
            mkavl_assert_abort(NULL != item);
        }
    }

    return (rc);
}

/**
 * Get the item at the end of the AVL tree.  That is, either the first or last
 * item in the tree rooted at the node depending on the side input.
 *
 * @param node The root of the subtree to search.
 * @param found_item  The item found.
 * @param side 0 to find the first (smallest) item or 1 to find the last
 * (largest) item
 * @return The return code
 */
static inline mkavl_rc_e
mkavl_avl_end_item (const struct avl_node *node, void **found_item,
                    uint8_t side)
{
    const struct avl_node *cur_node;

    if (NULL == found_item) {
        return (MKAVL_RC_E_EINVAL);
    }
    *found_item = NULL;

    if (side >= 2) {
        return (MKAVL_RC_E_EINVAL);
    }

    if (NULL == node) {
        return (MKAVL_RC_E_SUCCESS);
    }
    cur_node = node;

    while (NULL != cur_node->avl_link[side]) {
        cur_node = cur_node->avl_link[side];
    }
    *found_item = cur_node->avl_data;

    return (MKAVL_RC_E_SUCCESS);
}

/**
 * Get the first (smallest) item in the tree rooted at the node.
 *
 * @param node The root of the subtree.
 * @param found_item The item that was found.
 * @return The return code
 */
static mkavl_rc_e
mkavl_avl_first (const struct avl_node *node, void **found_item)
{
    return (mkavl_avl_end_item(node, found_item, 0));
}

/**
 * Get the last (largest) item in the tree rooted at the node.
 *
 * @param node The root of the subtree.
 * @param found_item The item that was found.
 * @return The return code
 */
static mkavl_rc_e
mkavl_avl_last (const struct avl_node *node, void **found_item)
{
    return (mkavl_avl_end_item(node, found_item, 1));
}

/**
 * Find the item less than (or equal to) the given lookup_item.  Note that the
 * lookup_item does not necessarily have to exist in the tree for an item to be
 * found.  This is a recursive funciton.
 *
 * @param avl_tree The AVL tree to search.
 * @param node The current node to compare.
 * @param find_equal_to Indicates whether to return an equal item if found or
 * only strictly less than.
 * @param lookup_item The item to use as the lookup target.
 * @param found_item The item found.
 * @return The return code
 */
static mkavl_rc_e
mkavl_find_avl_lt (const struct avl_table *avl_tree,
                   const struct avl_node *node,
                   bool find_equal_to, const void *lookup_item, 
                   void **found_item)
{
    int32_t cmp_rc;
    mkavl_rc_e rc;

    if ((NULL == found_item) || (NULL == lookup_item) ||
        (NULL == avl_tree)) {
        return (MKAVL_RC_E_EINVAL);
    }
    *found_item = NULL;

    if (NULL == node) {
        return (MKAVL_RC_E_SUCCESS);
    }

    cmp_rc = avl_tree->avl_compare(node->avl_data, lookup_item,
                                   avl_tree->avl_param);

    if (cmp_rc < 0) {
        /* Current node is less than what we're searching for */
        rc = mkavl_find_avl_lt(avl_tree, node->avl_link[1], find_equal_to,
                               lookup_item, found_item);
        if (mkavl_rc_e_is_notok(rc)) {
            return (rc);
        }

        if (NULL == *found_item) {
            /* 
             * There is nothing smaller in the right subtree, so the current
             * node is the winner.  Otherwise, return what was found in the
             * right subtree.
             */
            *found_item = node->avl_data;
        }

        return (MKAVL_RC_E_SUCCESS);
    } else if (cmp_rc > 0) {
        /* Current node is greater than what we're searching for */
        return (mkavl_find_avl_lt(avl_tree, node->avl_link[0], find_equal_to,
                                  lookup_item, found_item));
    }

    /* Current node is equal to what we're searching for */

    if (find_equal_to) {
        /* We're done if we can stop on equality */
        *found_item = node->avl_data;
        return (MKAVL_RC_E_SUCCESS);
    }

    /* 
     * If we can't stop on equality, then get the biggest item in the left
     * subtree.
     */
    return (mkavl_avl_last(node->avl_link[0], found_item));
}

/**
 * Find the item greater than (or equal to) the given lookup_item.  Note that
 * the lookup_item does not necessarily have to exist in the tree for an item to
 * be found.  This is a recursive funciton.
 *
 * @param avl_tree The AVL tree to search.
 * @param node The current node to compare.
 * @param find_equal_to Indicates whether to return an equal item if found or
 * only strictly greater than.
 * @param lookup_item The item to use as the lookup target.
 * @param found_item The item found.
 * @return The return code
 */
static mkavl_rc_e
mkavl_find_avl_gt (const struct avl_table *avl_tree,
                   const struct avl_node *node,
                   bool find_equal_to, const void *lookup_item, 
                   void **found_item)
{
    int32_t cmp_rc;
    mkavl_rc_e rc;

    if ((NULL == found_item) || (NULL == lookup_item) ||
        (NULL == avl_tree)) {
        return (MKAVL_RC_E_EINVAL);
    }
    *found_item = NULL;

    if (NULL == node) {
        return (MKAVL_RC_E_SUCCESS);
    }

    cmp_rc = avl_tree->avl_compare(node->avl_data, lookup_item,
                                   avl_tree->avl_param);

    if (cmp_rc < 0) {
        /* Current node is less than what we're searching for */
        return (mkavl_find_avl_gt(avl_tree, node->avl_link[1], find_equal_to,
                                  lookup_item, found_item));
    } else if (cmp_rc > 0) {
        /* Current node is greater than what we're searching for */
        rc = mkavl_find_avl_gt(avl_tree, node->avl_link[0], find_equal_to,
                               lookup_item, found_item);
        if (mkavl_rc_e_is_notok(rc)) {
            return (rc);
        }

        if (NULL == *found_item) {
            /* 
             * There is nothing bigger in the left subtree, so the current node
             * is the winner.  Otherwise, return what was found in the left
             * subtree.
             */
            *found_item = node->avl_data;
        }

        return (MKAVL_RC_E_SUCCESS);
    }

    /* Current node is equal to what we're searching for */

    if (find_equal_to) {
        /* We're done if we can stop on equality */
        *found_item = node->avl_data;
        return (MKAVL_RC_E_SUCCESS);
    }

    /* 
     * If we can't stop on equality, then get the smallest item in the right
     * subtree.
     */
    return (mkavl_avl_first(node->avl_link[1], found_item));
}

/**
 * Find an item in the tree.
 *
 * @param tree_h The tree to search
 * @param type The type of lookup to do.
 * @param key_idx The AVL tree being searched.
 * @param lookup_item The item to use as the lookup target.
 * @param found_item The item found for the lookup.
 * @return The return code
 */
mkavl_rc_e
mkavl_find (mkavl_tree_handle tree_h, mkavl_find_type_e type,
            size_t key_idx, const void *lookup_item, void **found_item)
{
    void *item = NULL;
    mkavl_rc_e rc;

    if ((NULL == lookup_item) || (NULL == found_item)) {
        return (MKAVL_RC_E_EINVAL);
    }
    *found_item = NULL;

    if (!mkavl_find_type_e_is_valid(type)) {
        return (MKAVL_RC_E_EINVAL);
    }

    if (!mkavl_tree_is_valid(tree_h)) {
        return (MKAVL_RC_E_EINVAL);
    }

    if (key_idx >= tree_h->avl_tree_count) {
        return (MKAVL_RC_E_EINVAL);
    }

    switch (type) {
    case MKAVL_FIND_TYPE_E_EQUAL:
        item = avl_find(tree_h->avl_tree_array[key_idx].tree, lookup_item);
        break;
    case MKAVL_FIND_TYPE_E_GT:
        rc = mkavl_find_avl_gt(tree_h->avl_tree_array[key_idx].tree,
                               tree_h->avl_tree_array[key_idx].tree->avl_root,
                               false, lookup_item, &item);
        if (mkavl_rc_e_is_notok(rc)) {
            return (rc);
        }
        break;
    case MKAVL_FIND_TYPE_E_GE:
        rc = mkavl_find_avl_gt(tree_h->avl_tree_array[key_idx].tree,
                               tree_h->avl_tree_array[key_idx].tree->avl_root,
                               true, lookup_item, &item);
        if (mkavl_rc_e_is_notok(rc)) {
            return (rc);
        }
        break;
    case MKAVL_FIND_TYPE_E_LT:
        rc = mkavl_find_avl_lt(tree_h->avl_tree_array[key_idx].tree,
                               tree_h->avl_tree_array[key_idx].tree->avl_root,
                               false, lookup_item, &item);
        if (mkavl_rc_e_is_notok(rc)) {
            return (rc);
        }
        break;
    case MKAVL_FIND_TYPE_E_LE:
        rc = mkavl_find_avl_lt(tree_h->avl_tree_array[key_idx].tree,
                               tree_h->avl_tree_array[key_idx].tree->avl_root,
                               true, lookup_item, &item);
        if (mkavl_rc_e_is_notok(rc)) {
            return (rc);
        }
        break;
    default:
        return (MKAVL_RC_E_EINVAL);
    }

    *found_item = item;

    return (MKAVL_RC_E_SUCCESS);
}

/**
 * Remove an item from mkavl tree.
 *
 * @param tree_h The tree from which to remove.
 * @param item_to_remove The item being removed.
 * @param found_item If the item existed, the item that was found and removed.
 * @return The return code
 */
mkavl_rc_e
mkavl_remove (mkavl_tree_handle tree_h, const void *item_to_remove,
              void **found_item)
{
    uint32_t i, err_idx = 0;
    void *item, *first_item = NULL;
    mkavl_rc_e rc = MKAVL_RC_E_SUCCESS;

    if ((NULL == item_to_remove) || (NULL == found_item)) {
        return (MKAVL_RC_E_EINVAL);
    }
    *found_item = NULL;

    if (!mkavl_tree_is_valid(tree_h)) {
        return (MKAVL_RC_E_EINVAL);
    }

    for (i = 0; i < tree_h->avl_tree_count; ++i) {
        item = avl_delete(tree_h->avl_tree_array[i].tree, item_to_remove);
        if (0 == i) {
            first_item = item;
        } else if (first_item != item) {
            err_idx = i + 1;
            rc = MKAVL_RC_E_EOOSYNC;
            goto err_exit;
        }
    }

    if (NULL != first_item) {
        if (0 == tree_h->item_count) {
            rc = MKAVL_RC_E_EOOSYNC;
            goto err_exit;
        }
        --(tree_h->item_count);
    }

    *found_item = first_item;

    return (rc);

err_exit:

    for (i = 0; i < err_idx; ++i) {
        if (NULL != first_item) {
            /* Attempt to insert all the items we removed */
            item = avl_insert(tree_h->avl_tree_array[i].tree, first_item);
            mkavl_assert_abort(NULL == item);
        }
    }

    return (rc);
}

/**
 * Add an item only to a specific AVL tree within the mkavl tree.  Note that
 * this must be used very carefully in conjunction with mkavl_remove_key_idx to
 * make sure the mkavl does not become out of sync.  In steady state, the mkavl
 * should always have an equal number of data items in each AVL tree.  However,
 * if a field changes in an item that affects some keys but not other, these
 * APIs may be used to remove the item with the old values from the affected
 * AVLs, update the values, and then re-add to each AVL tree from which the old
 * item was removed.
 *
 * @see mkavl_remove_key_idx
 * @param tree_h The mkavl tree.
 * @param key_idx The index of the AVL tree to which the item is added.
 * @param item_to_add The item being added.
 * @param existing_item If an existing item is found, it is returned.
 * @return The return code
 */
mkavl_rc_e
mkavl_add_key_idx (mkavl_tree_handle tree_h, size_t key_idx,
                   void *item_to_add, void **existing_item)
{
    void *item;

    if ((NULL == item_to_add) || (NULL == existing_item)) {
        return (MKAVL_RC_E_EINVAL);
    }
    *existing_item = NULL;

    if (!mkavl_tree_is_valid(tree_h)) {
        return (MKAVL_RC_E_EINVAL);
    }

    if (key_idx >= tree_h->avl_tree_count) {
        return (MKAVL_RC_E_EINVAL);
    }

    item = avl_insert(tree_h->avl_tree_array[key_idx].tree, item_to_add);
    *existing_item = item;

    return (MKAVL_RC_E_SUCCESS);
}

/**
 * Remove an item only from a specific AVL tree within the mkavl tree.  Note
 * that this must be used very carefully in conjunction with
 * mkavl_add_key_idx to make sure the mkavl does not become out of sync.  In
 * steady state, the mkavl should always have an equal number of data items in
 * each AVL tree.  However, if a field changes in an item that affects some keys
 * but not other, these APIs may be used to remove the item with the old values
 * from the affected AVLs, update the values, and then re-add to each AVL tree
 * from which the old item was removed.
 *
 * @see mkavl_add_key_idx
 * @param tree_h The mkavl tree.
 * @param key_idx The index of the AVL tree to which the item is added.
 * @param item_to_remove The item being removed.
 * @param found_item If the item existed, the item that was found and removed.
 * @return The return code
 */
mkavl_rc_e
mkavl_remove_key_idx (mkavl_tree_handle tree_h, size_t key_idx,
                      const void *item_to_remove, void **found_item)
{
    void *item;

    if ((NULL == item_to_remove) || (NULL == found_item)) {
        return (MKAVL_RC_E_EINVAL);
    }
    *found_item = NULL;

    if (!mkavl_tree_is_valid(tree_h)) {
        return (MKAVL_RC_E_EINVAL);
    }

    if (key_idx >= tree_h->avl_tree_count) {
        return (MKAVL_RC_E_EINVAL);
    }

    item = avl_delete(tree_h->avl_tree_array[key_idx].tree, item_to_remove);
    *found_item = item;

    return (MKAVL_RC_E_SUCCESS);
}

/**
 * Get a count of the total number of items within the tree.  Note that this is
 * the steady state account, i.e., broadly, the number of calls to mkavl_add
 * that succeeded minus the number of calls to mkavl_remove that succeeded.  If
 * this is called in between calls to mkavl_add_key_idx and
 * mkavl_remove_key_idx, then it is possible for individual AVL trees to have a
 * different count.
 *
 * @see mkavl_add
 * @see mkavl_remove
 * @see mkavl_add_key_idx
 * @see mkavl_remove_key_idx
 */
uint32_t
mkavl_count (mkavl_tree_handle tree_h)
{
    if (!mkavl_tree_is_valid(tree_h)) {
        return (0);
    }

    return (tree_h->item_count);
}

/**
 * Walk every node in the tree, calling the given function for each item.
 *
 * @param tree_h The tree to walk.
 * @param cb_fn The callback function to apply to each item.
 * @param walk_context The opaque walk context passed to the callback.
 * @return The return code.
 */
mkavl_rc_e
mkavl_walk (mkavl_tree_handle tree_h, mkavl_walk_cb_fn cb_fn,
            void *walk_context)
{
    void *item;
    bool stop_walk = false;
    struct avl_traverser avl_t = {0};
    mkavl_rc_e rc = MKAVL_RC_E_SUCCESS;

    if (NULL == cb_fn) {
        return (MKAVL_RC_E_EINVAL);
    }

    if (!mkavl_tree_is_valid(tree_h)) {
        return (MKAVL_RC_E_EINVAL);
    }

    /* 
     * Just randomly select the first tree as the one on which to iterate.
     * All trees should have the same set of data, just in different orders.
     */
    avl_t_init(&avl_t, tree_h->avl_tree_array[0].tree);

    item = avl_t_first(&avl_t, tree_h->avl_tree_array[0].tree);
    while ((NULL != item) && !stop_walk) {
        rc = cb_fn(item, tree_h->context, walk_context, &stop_walk);
        if (mkavl_rc_e_is_notok(rc)) {
            break;
        }
        item = avl_t_next(&avl_t);
    }

    return (rc);
}

/**
 * Create a new iterator to use.
 *
 * @see mkavl_iter_delete
 * @param iterator_h The pointer to fill in with the new iterator.
 * @param tree_h The tree on which to iterate.
 * @param key_idx The index of the AVL tree on which to iterate.
 * @return The return code
 */
mkavl_rc_e
mkavl_iter_new (mkavl_iterator_handle *iterator_h, mkavl_tree_handle tree_h,
                size_t key_idx)
{
    mkavl_iterator_handle local_iter_h;
    mkavl_rc_e rc;

    if ((NULL == iterator_h) || !mkavl_tree_is_valid(tree_h)) {
        return (MKAVL_RC_E_EINVAL);
    }

    if (key_idx >= tree_h->avl_tree_count) {
        return (MKAVL_RC_E_EINVAL);
    }
    *iterator_h = NULL;

    local_iter_h =
        tree_h->allocator.mkavl_allocator.malloc_fn(sizeof(*local_iter_h));
    if (NULL == local_iter_h) {
        rc = MKAVL_RC_E_ENOMEM;
        goto err_exit;
    }
    local_iter_h->tree_h = tree_h;
    local_iter_h->key_idx = key_idx;
    memset(&(local_iter_h->avl_t), 0, sizeof(local_iter_h->avl_t));

    avl_t_init(&(local_iter_h->avl_t), tree_h->avl_tree_array[key_idx].tree);

    *iterator_h = local_iter_h;

    return (MKAVL_RC_E_SUCCESS);

err_exit:

    if (NULL != local_iter_h) {
        tree_h->allocator.mkavl_allocator.free_fn(local_iter_h);
        local_iter_h = NULL;
    }

    return (rc);
}

/**
 * Destroy the iterator.
 *
 * @see mkavl_iter_new
 * @param iterator_h The iterator to free.  Upon return, this will be set to
 * NULL.
 * @return The return code
 */
mkavl_rc_e
mkavl_iter_delete (mkavl_iterator_handle *iterator_h)
{
    mkavl_iterator_handle local_iter_h;
    mkavl_free_fn free_fn;

    if (NULL == iterator_h) {
        return (MKAVL_RC_E_EINVAL);
    }
    local_iter_h = *iterator_h;

    if (!mkavl_iterator_is_valid(local_iter_h)) {
        return (MKAVL_RC_E_EINVAL);
    }

    free_fn = local_iter_h->tree_h->allocator.mkavl_allocator.free_fn;
    free_fn(local_iter_h);

    *iterator_h = NULL;

    return (MKAVL_RC_E_SUCCESS);
}

/**
 * Get the first item in the iteration.
 *
 * @see mkavl_iter_new
 * @param iterator_h The iterator to use.
 * @param item The item found.
 * @return The return code
 */
mkavl_rc_e
mkavl_iter_first (mkavl_iterator_handle iterator_h, void **item)
{
    if (NULL == item) {
        return (MKAVL_RC_E_EINVAL);
    }
    *item = NULL;

    if (!mkavl_iterator_is_valid(iterator_h)) {
        return (MKAVL_RC_E_EINVAL);
    }

    *item = avl_t_first(&(iterator_h->avl_t),
                iterator_h->tree_h->avl_tree_array[iterator_h->key_idx].tree);

    return (MKAVL_RC_E_SUCCESS);
}

/**
 * Get the last item in the iteration.
 *
 * @see mkavl_iter_new
 * @param iterator_h The iterator to use.
 * @param item The item found.
 * @return The return code
 */
mkavl_rc_e
mkavl_iter_last (mkavl_iterator_handle iterator_h, void **item)
{
    if (NULL == item) {
        return (MKAVL_RC_E_EINVAL);
    }
    *item = NULL;

    if (!mkavl_iterator_is_valid(iterator_h)) {
        return (MKAVL_RC_E_EINVAL);
    }

    *item = avl_t_last(&(iterator_h->avl_t),
                iterator_h->tree_h->avl_tree_array[iterator_h->key_idx].tree);

    return (MKAVL_RC_E_SUCCESS);
}

/**
 * Find an item and update the iterator to that node.
 *
 * @see mkavl_iter_new
 * @param iterator_h The iterator to use.
 * @param lookup_item The item to find.
 * @param found_item The item found.
 * @return The return code
 */
mkavl_rc_e
mkavl_iter_find (mkavl_iterator_handle iterator_h, void *lookup_item,
                 void **found_item)
{
    if ((NULL == lookup_item) || (NULL == found_item)) {
        return (MKAVL_RC_E_EINVAL);
    }
    *found_item = NULL;

    if (!mkavl_iterator_is_valid(iterator_h)) {
        return (MKAVL_RC_E_EINVAL);
    }

    *found_item = 
        avl_t_find(&(iterator_h->avl_t),
                   iterator_h->tree_h->avl_tree_array[iterator_h->key_idx].tree,
                   lookup_item);

    return (MKAVL_RC_E_SUCCESS);
}

/**
 * Get the next item in the iteration and update the iterator to that node.
 *
 * @see mkavl_iter_new
 * @param iterator_h The iterator to use.
 * @param item The item found.
 * @return The return code
 */
mkavl_rc_e
mkavl_iter_next (mkavl_iterator_handle iterator_h, void **item)
{
    if (NULL == item) {
        return (MKAVL_RC_E_EINVAL);
    }
    *item = NULL;

    if (!mkavl_iterator_is_valid(iterator_h)) {
        return (MKAVL_RC_E_EINVAL);
    }

    *item = avl_t_next(&(iterator_h->avl_t));

    return (MKAVL_RC_E_SUCCESS);
}

/**
 * Get the previous item in the iteration and update the iterator to that node.
 *
 * @see mkavl_iter_new
 * @param iterator_h The iterator to use.
 * @param item The item found.
 * @return The return code
 */
mkavl_rc_e
mkavl_iter_prev (mkavl_iterator_handle iterator_h, void **item)
{
    if (NULL == item) {
        return (MKAVL_RC_E_EINVAL);
    }
    *item = NULL;

    if (!mkavl_iterator_is_valid(iterator_h)) {
        return (MKAVL_RC_E_EINVAL);
    }

    *item = avl_t_prev(&(iterator_h->avl_t));

    return (MKAVL_RC_E_SUCCESS);
}

/**
 * Get the current item in the iteration.
 *
 * @see mkavl_iter_new
 * @param iterator_h The iterator to use.
 * @param item The item found.
 * @return The return code
 */
mkavl_rc_e
mkavl_iter_cur (mkavl_iterator_handle iterator_h, void **item)
{
    if (NULL == item) {
        return (MKAVL_RC_E_EINVAL);
    }
    *item = NULL;

    if (!mkavl_iterator_is_valid(iterator_h)) {
        return (MKAVL_RC_E_EINVAL);
    }

    *item = avl_t_cur(&(iterator_h->avl_t));

    return (MKAVL_RC_E_SUCCESS);
}
