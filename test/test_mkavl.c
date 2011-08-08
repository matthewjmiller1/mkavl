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
 * Unit test for mkavl library.
 */

#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include "../mkavl.h"

#define LOG_FAIL(fmt, args...) \
do { \
    printf("FAILURE(%s:%u): " fmt "\n", __FUNCTION__, __LINE__, ##args); \
} while (0)

/**
 * Determine the number of elements in an array.
 */
#ifndef NELEMS
#define NELEMS(x) (sizeof(x) / sizeof(x[0]))
#endif

#ifndef CT_ASSERT
#define CT_ASSERT(e) extern char (*CT_ASSERT(void)) [sizeof(char[1 - 2*!(e)])]
#endif

static const uint32_t default_node_cnt = 15;
static const uint32_t default_run_cnt = 15;
static const uint8_t default_verbosity = 0;
static const uint32_t default_range_start = 0;
static const uint32_t default_range_end = 100;

typedef struct test_mkavl_opts_st_ {
    uint32_t node_cnt;
    uint32_t run_cnt;
    uint32_t seed;
    uint8_t verbosity;
    uint32_t range_start;
    uint32_t range_end;
} test_mkavl_opts_st;

static void
print_usage (bool do_exit, int32_t exit_val)
{
    printf("\nTest the mkavl structure\n\n");
    printf("Usage:\n");
    printf("-s <seed>\n"
           "   The starting seed for the RNG (default=seeded by time()).\n");
    printf("-n <nodes>\n"
           "   The number of nodes to place in the trees (default=%u).\n",
           default_node_cnt);
    printf("-b <range beginning>\n"
           "   The smallest (inclusive ) possible data value in the "
           "range of values (default=%u).\n", default_range_start);
    printf("-e <range ending>\n"
           "   The largest (exclusive) possible data value in the "
           "range of values (default=%u).\n", default_range_end);
    printf("-r <runs>\n"
           "   The number of runs to do (default=%u).\n",
           default_run_cnt);
    printf("-v <verbosity level>\n"
           "   A higher number gives more output (default=%u).\n",
           default_verbosity);
    printf("\n");

    if (do_exit) {
        exit(exit_val);
    }
}

static void
print_opts (test_mkavl_opts_st *opts)
{
    if (NULL == opts) {
        return;
    }

    printf("test_mkavl_opts: seed=%u, node_cnt=%u, run_cnt=%u,\n"
           "                 range=[%u,%u) verbosity=%u\n",
           opts->seed, opts->node_cnt, opts->run_cnt, opts->range_start,
           opts->range_end, opts->verbosity);
}

static void
parse_command_line (int argc, char **argv, test_mkavl_opts_st *opts)
{
    int c;
    char *end_ptr;
    uint32_t val;

    if (NULL == opts) {
        return;
    }

    opts->node_cnt = default_node_cnt;
    opts->run_cnt = default_run_cnt;
    opts->verbosity = default_verbosity;
    opts->range_start = default_range_start;
    opts->range_end = default_range_end;
    opts->seed = (uint32_t) time(NULL);

    while ((c = getopt(argc, argv, "n:r:v:s:b:e:h")) != -1) {
        switch (c) {
        case 'n':
            val = strtol(optarg, &end_ptr, 10);
            if ((end_ptr != optarg) && (0 == errno)) {
                opts->node_cnt = val;
            }
            break;
        case 'r':
            val = strtol(optarg, &end_ptr, 10);
            if ((end_ptr != optarg) && (0 == errno)) {
                opts->run_cnt = val;
            }
            break;
        case 'v':
            val = strtol(optarg, &end_ptr, 10);
            if ((end_ptr != optarg) && (0 == errno)) {
                opts->verbosity = val;
            }
            break;
        case 's':
            val = strtol(optarg, &end_ptr, 10);
            if ((end_ptr != optarg) && (0 == errno)) {
                opts->seed = val;
            }
            break;
        case 'b':
            val = strtol(optarg, &end_ptr, 10);
            if ((end_ptr != optarg) && (0 == errno)) {
                opts->range_start = val;
            }
        case 'e':
            val = strtol(optarg, &end_ptr, 10);
            if ((end_ptr != optarg) && (0 == errno)) {
                opts->range_end = val;
            }
            break;
        case 'h':
        case '?':
        default:
            print_usage(true, EXIT_SUCCESS);
            break;
        }
    }

    if (optind < argc) {
        print_usage(true, EXIT_SUCCESS);
    }

    if (opts->range_start >= opts->range_end) {
        printf("Error: range start(%u) must be strictly less than range "
               "end(%u)\n", opts->range_start, opts->range_end);
        print_usage(true, EXIT_SUCCESS);
    }

    if (0 == opts->node_cnt) {
        printf("Error: node count(%u) must be non-zero\n",
               opts->node_cnt);
        print_usage(true, EXIT_SUCCESS);
    }

    if (opts->verbosity >= 3) {
        print_opts(opts);
    }
}

static void
permute_array (const uint32_t *src_array, uint32_t *dst_array, 
               size_t num_elem)
{
    uint32_t i, j, swp_val;

    if ((NULL == src_array) || (NULL == dst_array) || (0 == num_elem)) {
        return;
    }

    memcpy(dst_array, src_array, (num_elem * sizeof(*dst_array)));

    for (i = (num_elem - 1); i > 0; --i) {
        j = (rand() % (i + 1));
        swp_val = dst_array[j];
        dst_array[j] = dst_array[i];
        dst_array[i] = swp_val;
    }

}

static int
uint32_t_cmp (const void *a, const void *b)
{
    const uint32_t i1 = *((const uint32_t *) a);
    const uint32_t i2 = *((const uint32_t *) b);

    if (i1 < i2) {
        return (-1);
    } else if (i1 > i2) {
        return (1);
    }

    return (0);
}

/*
 * Assumes array is sorted.
 */
static uint32_t
get_unique_count (uint32_t *array, size_t num_elem)
{
    uint32_t count = 0;
    uint32_t cur_val;
    uint32_t i;

    if ((NULL == array) || (0 == num_elem)) {
        return (0);
    }

    for (i = 0; i < num_elem; ++i) {
        if ((0 == i) || (array[i] != cur_val)) {
            cur_val = array[i];
            ++count;
        }
    }

    return (count);
}

typedef struct mkavl_test_input_st_ {
    uint32_t *insert_seq;
    uint32_t *delete_seq;
    uint32_t *sorted_seq;
    uint32_t uniq_cnt; 
    uint32_t dup_cnt; 
    const test_mkavl_opts_st *opts;
    mkavl_tree_handle tree_h;
    mkavl_tree_handle tree_copy_h;
} mkavl_test_input_st;

/* 
 * Do a forward declaration so we can keep all the AVL setup stuff separate
 * below main().
 */
static bool
run_mkavl_test(mkavl_test_input_st *input);

/**
 * Main function to test objects.
 */
int 
main (int argc, char *argv[])
{
    test_mkavl_opts_st opts;
    bool was_success;
    uint32_t fail_count = 0;
    uint32_t i;
    uint32_t cur_run, cur_seed, uniq_cnt;
    mkavl_test_input_st test_input = {0};

    parse_command_line(argc, argv, &opts);

    printf("\n");
    cur_seed = opts.seed;
    for (cur_run = 0; cur_run < opts.run_cnt; ++cur_run) {
        uint32_t insert_seq[opts.node_cnt];
        uint32_t delete_seq[opts.node_cnt];
        uint32_t sorted_seq[opts.node_cnt];

        printf("Doing run %u with seed %u\n", (cur_run + 1), cur_seed);
        srand(cur_seed);

        for (i = 0; i < opts.node_cnt; ++i) {
            insert_seq[i] = ((rand() % opts.range_end) + opts.range_start);
        }
        permute_array(insert_seq, delete_seq, opts.node_cnt);
        memcpy(sorted_seq, insert_seq, sizeof(sorted_seq));
        qsort(sorted_seq, opts.node_cnt, sizeof(sorted_seq[0]), uint32_t_cmp);
        uniq_cnt = get_unique_count(sorted_seq, opts.node_cnt);

        if (opts.verbosity >= 5) {
            printf("Unique count: %u\n", uniq_cnt);
            printf("Insertion sequence:\n  ");
            for (i = 0; i < opts.node_cnt; ++i) {
                printf(" %u", insert_seq[i]);
            }
            printf("\n");

            printf("Deletion sequence:\n  ");
            for (i = 0; i < opts.node_cnt; ++i) {
                printf(" %u", delete_seq[i]);
            }
            printf("\n");
        }

        test_input.insert_seq = insert_seq;
        test_input.delete_seq = delete_seq;
        test_input.sorted_seq = sorted_seq;
        test_input.uniq_cnt = uniq_cnt;
        test_input.dup_cnt = (opts.node_cnt - uniq_cnt);
        test_input.opts = &opts;
        test_input.tree_h = NULL;
        test_input.tree_copy_h = NULL;

        was_success = run_mkavl_test(&test_input);
        if (!was_success) {
            printf("FAILURE: the test has failed for seed %u!!!\n", cur_seed);
            ++fail_count;
        }

        ++cur_seed;
    }

    if (0 != fail_count) {
        printf("\n%u/%u TESTS FAILED\n", fail_count, opts.run_cnt);
    } else {
        printf("\nALL TESTS PASSED\n");
    }
    printf("\n");

    return (0);
}

/* AVL operation functions */

#define TEST_MKAVL_MAGIC 0x1234ABCD

/**
 * Simply compare the uint32_t values.
 *
 * @param item1 Item to compare
 * @param item2 Item to compare
 * @param context Context for the tree
 * @return Comparison result
 */
static int32_t
mkavl_cmp_fn1 (const void *item1, const void *item2, void *context)
{
    const uint32_t *i1 = item1;
    const uint32_t *i2 = item2;

    if (TEST_MKAVL_MAGIC != (uintptr_t) context) {
        abort();
    }

    if (*i1 < *i2) {
        return (-1);
    } else if (*i1 > *i2) {
        return (1);
    }

    return (0);
}

/**
 * Reverse the values of the items.
 *
 * @param item1 Item to compare
 * @param item2 Item to compare
 * @param context Context for the tree
 * @return Comparison result
 */
static int32_t
mkavl_cmp_fn2 (const void *item1, const void *item2, void *context)
{
    const uint32_t *i1 = item1;
    const uint32_t *i2 = item2;

    if (TEST_MKAVL_MAGIC != (uintptr_t) context) {
        abort();
    }

    if (*i1 > *i2) {
        return (-1);
    } else if (*i1 < *i2) {
        return (1);
    }

    return (0);
}

typedef enum mkavl_test_key_e_ {
    MKAVL_TEST_KEY_E_ASC,
    MKAVL_TEST_KEY_E_DESC,
    MKAVL_TEST_KEY_E_MAX,
} mkavl_test_key_e;

const static mkavl_test_key_e mkavl_key_opposite[] = {
    MKAVL_TEST_KEY_E_DESC,
    MKAVL_TEST_KEY_E_ASC,
};

mkavl_compare_fn cmp_fn_array[] = { mkavl_cmp_fn1 , mkavl_cmp_fn2 };

CT_ASSERT(NELEMS(mkavl_key_opposite) == MKAVL_TEST_KEY_E_MAX);
CT_ASSERT(NELEMS(cmp_fn_array) == MKAVL_TEST_KEY_E_MAX);

static bool
mkavl_test_new_error (void)
{
    mkavl_rc_e rc;
    mkavl_tree_handle tree_h = NULL;

    if (0 != mkavl_count(NULL)) {
        LOG_FAIL("NULL mkavl_count failed, mkavl_count(%u)", 
                 mkavl_count(NULL));
        return (false);
    }

    rc = mkavl_new(NULL, cmp_fn_array, NELEMS(cmp_fn_array),
                   (void *) TEST_MKAVL_MAGIC, NULL);
    if (mkavl_rc_e_is_ok(rc)) {
        LOG_FAIL("NULL tree failed, rc(%s)", mkavl_rc_e_get_string(rc));
        return (false);
    }

    rc = mkavl_new(&tree_h, NULL, NELEMS(cmp_fn_array),
                   (void *) TEST_MKAVL_MAGIC, NULL);
    if (mkavl_rc_e_is_ok(rc)) {
        LOG_FAIL("NULL function failed, rc(%s)", mkavl_rc_e_get_string(rc));
        return (false);
    }

    rc = mkavl_new(&tree_h, cmp_fn_array, 0,
                   (void *) TEST_MKAVL_MAGIC, NULL);
    if (mkavl_rc_e_is_ok(rc)) {
        LOG_FAIL("zero size function failed, rc(%s)",
                 mkavl_rc_e_get_string(rc));
        return (false);
    }

    return (true);
}

static bool
mkavl_test_new (mkavl_test_input_st *input, mkavl_allocator_st *allocator)
{
    mkavl_rc_e rc;

    rc = mkavl_new(&(input->tree_h), cmp_fn_array, NELEMS(cmp_fn_array),
                   (void *) TEST_MKAVL_MAGIC, allocator);
    if (mkavl_rc_e_is_notok(rc)) {
        LOG_FAIL("new failed, rc(%s)", mkavl_rc_e_get_string(rc));
        return (false);
    }

    return (true);
}

static bool
mkavl_test_delete (mkavl_test_input_st *input, mkavl_item_fn item_fn)
{
    mkavl_rc_e rc;

    rc = mkavl_delete(&(input->tree_h), item_fn);
    if (mkavl_rc_e_is_notok(rc)) {
        LOG_FAIL("delete empty failed, rc(%s)", mkavl_rc_e_get_string(rc));
        return (false);
    }

    return (true);
}

static bool
mkavl_test_add_error (mkavl_test_input_st *input)
{
    uint32_t *existing_item;
    mkavl_rc_e rc;

    rc = mkavl_add(input->tree_h, &(input->insert_seq[0]), (void **) NULL);
    if (mkavl_rc_e_is_ok(rc)) {
        LOG_FAIL("NULL existing item failed, rc(%s)",
                 mkavl_rc_e_get_string(rc));
        return (false);
    }

    rc = mkavl_add(input->tree_h, NULL, (void **) &existing_item);
    if (mkavl_rc_e_is_ok(rc)) {
        LOG_FAIL("NULL item failed, rc(%s)",
                 mkavl_rc_e_get_string(rc));
        return (false);
    }

    rc = mkavl_add(NULL, &(input->insert_seq[0]), (void **) &existing_item);
    if (mkavl_rc_e_is_ok(rc)) {
        LOG_FAIL("NULL tree failed, rc(%s)", mkavl_rc_e_get_string(rc));
        return (false);
    }

    return (true);
}

static bool
mkavl_test_add (mkavl_test_input_st *input)
{
    uint32_t i;
    uint32_t non_null_cnt = 0;
    uint32_t *existing_item;
    mkavl_rc_e rc;

    for (i = 0; i < input->opts->node_cnt; ++i) {
        rc = mkavl_add(input->tree_h, &(input->insert_seq[i]), 
                       (void **) &existing_item);
        if (mkavl_rc_e_is_notok(rc)) {
            LOG_FAIL("add failed, rc(%s)", mkavl_rc_e_get_string(rc));
            return (false);
        }

        if (NULL != existing_item) {
            ++non_null_cnt;
        }
    }

    if (non_null_cnt != input->dup_cnt) {
        LOG_FAIL("duplidate check failed, non_null_cnt(%u) dup_cnt(%u)", 
                 non_null_cnt, input->dup_cnt);
        return (false);
    }

    if (mkavl_count(input->tree_h) != input->uniq_cnt) {
        LOG_FAIL("unique check failed, mkavl_count(%u) uniq_cnt(%u)", 
                 mkavl_count(input->tree_h), input->uniq_cnt);
        return (false);
    }

    return (true);
}

static bool
mkavl_test_find_equal (mkavl_test_input_st *input)
{
    uint32_t *existing_item;
    uint32_t i, j;
    mkavl_rc_e rc;

    for (i = 0; i < input->opts->node_cnt; ++i) {
        for (j = 0; j < MKAVL_TEST_KEY_E_MAX; ++j) {
            rc = mkavl_find(input->tree_h, MKAVL_FIND_TYPE_E_EQUAL, j,
                            &(input->insert_seq[i]), (void **) &existing_item);
            if (mkavl_rc_e_is_notok(rc)) {
                LOG_FAIL("find failed, rc(%s)", mkavl_rc_e_get_string(rc));
                return (false);
            }

            if (NULL == existing_item) {
                LOG_FAIL("find failed for %u", input->insert_seq[i]);
                return (false);
            }

            if (*existing_item != input->insert_seq[i]) {
                LOG_FAIL("find failed for %u, found %u", input->insert_seq[i],
                         *existing_item);
                return (false);
            }
        }
    }
    
    return (true);
}

static bool
mkavl_test_find_greater (mkavl_test_input_st *input, bool do_equal)
{
    mkavl_find_type_e type;
    uint32_t *existing_item;
    uint32_t rand_lookup_item;
    uint32_t i, j;
    mkavl_rc_e rc;

    type = MKAVL_FIND_TYPE_E_GT;
    if (do_equal) {
        type = MKAVL_FIND_TYPE_E_GE;
    }

    for (i = 0; i < input->opts->node_cnt; ++i) {
        for (j = 0; j < MKAVL_TEST_KEY_E_MAX; ++j) {
            /* Do the operation on an existing item */
            rc = mkavl_find(input->tree_h, type, j,
                            &(input->insert_seq[i]), (void **) &existing_item);
            if (mkavl_rc_e_is_notok(rc)) {
                LOG_FAIL("find failed, rc(%s)", mkavl_rc_e_get_string(rc));
                return (false);
            }

            if (do_equal) {
                if (NULL == existing_item) {
                    LOG_FAIL("find failed for %u", input->insert_seq[i]);
                    return (false);
                }

                if (*existing_item != input->insert_seq[i]) {
                    LOG_FAIL("find failed for %u, found %u",
                             input->insert_seq[i],
                             *existing_item);
                    return (false);
                }
            } else {
                // TODO
            }

            /* Do the operation on a (potentially) non-existing item */
            rand_lookup_item = ((rand() % input->opts->range_end) + 
                                input->opts->range_start);
            rc = mkavl_find(input->tree_h, type, j,
                            &rand_lookup_item, (void **) &existing_item);
            if (mkavl_rc_e_is_notok(rc)) {
                LOG_FAIL("find failed, rc(%s)", mkavl_rc_e_get_string(rc));
                return (false);
            }
            // TODO
        }
    }
    
    return (true);
}

static bool
mkavl_test_find_error (mkavl_test_input_st *input)
{
    uint32_t *existing_item;
    mkavl_rc_e rc;

    rc = mkavl_find(NULL, MKAVL_FIND_TYPE_E_EQUAL, MKAVL_TEST_KEY_E_ASC,
                    &(input->insert_seq[0]), (void **) &existing_item);
    if (mkavl_rc_e_is_ok(rc)) {
        LOG_FAIL("NULL tree failed, rc(%s)", mkavl_rc_e_get_string(rc));
        return (false);
    }

    rc = mkavl_find(input->tree_h, (MKAVL_FIND_TYPE_E_MAX + 1), 
                    MKAVL_TEST_KEY_E_ASC,
                    &(input->insert_seq[0]), (void **) &existing_item);
    if (mkavl_rc_e_is_ok(rc)) {
        LOG_FAIL("Invalid type failed, rc(%s)", mkavl_rc_e_get_string(rc));
        return (false);
    }

    rc = mkavl_find(input->tree_h, MKAVL_FIND_TYPE_E_EQUAL, 
                    MKAVL_TEST_KEY_E_MAX,
                    &(input->insert_seq[0]), (void **) &existing_item);
    if (mkavl_rc_e_is_ok(rc)) {
        LOG_FAIL("Invalid key index failed, rc(%s)", mkavl_rc_e_get_string(rc));
        return (false);
    }

    rc = mkavl_find(input->tree_h, MKAVL_FIND_TYPE_E_EQUAL,
                    MKAVL_TEST_KEY_E_ASC, NULL, (void **) &existing_item);
    if (mkavl_rc_e_is_ok(rc)) {
        LOG_FAIL("NULL item failed, rc(%s)", mkavl_rc_e_get_string(rc));
        return (false);
    }

    rc = mkavl_find(input->tree_h, MKAVL_FIND_TYPE_E_EQUAL,
                    MKAVL_TEST_KEY_E_ASC, &(input->insert_seq[0]), NULL);
    if (mkavl_rc_e_is_ok(rc)) {
        LOG_FAIL("NULL item failed, rc(%s)", mkavl_rc_e_get_string(rc));
        return (false);
    }

    return (true);
}

static bool
mkavl_test_add_remove_key (mkavl_test_input_st *input)
{
    uint32_t i, j;
    uint32_t non_null_cnt, null_cnt;
    uint32_t *existing_item;
    mkavl_rc_e rc;

    for (i = 0; i < MKAVL_TEST_KEY_E_MAX; ++i) {
        /* Take them all out for one key */
        non_null_cnt = 0;
        for (j = 0; j < input->opts->node_cnt; ++j) {
            rc = mkavl_remove_key_idx(input->tree_h, i, 
                                      &(input->delete_seq[j]), 
                                      (void **) &existing_item);
            if (mkavl_rc_e_is_notok(rc)) {
                LOG_FAIL("remove key idx failed, rc(%s)", 
                         mkavl_rc_e_get_string(rc));
                return (false);
            }

            if (NULL != existing_item) {
                ++non_null_cnt;
            }

            rc = mkavl_find(input->tree_h, MKAVL_FIND_TYPE_E_EQUAL, i,
                            &(input->delete_seq[j]), (void **) &existing_item);
            if (mkavl_rc_e_is_notok(rc)) {
                LOG_FAIL("find failed, rc(%s)", mkavl_rc_e_get_string(rc));
                return (false);
            }

            if (NULL != existing_item) {
                LOG_FAIL("found item expected to be deleted, %u", 
                         input->delete_seq[j]);
                return (false);
            }

            rc = mkavl_find(input->tree_h, MKAVL_FIND_TYPE_E_EQUAL, 
                            mkavl_key_opposite[i],
                            &(input->delete_seq[j]), (void **) &existing_item);
            if (mkavl_rc_e_is_notok(rc)) {
                LOG_FAIL("find failed, rc(%s)", mkavl_rc_e_get_string(rc));
                return (false);
            }

            if (NULL == existing_item) {
                LOG_FAIL("did not find item, %u", input->delete_seq[j]);
                return (false);
            }
        }

        if (non_null_cnt != input->uniq_cnt) {
            LOG_FAIL("unique check failed, non_null_cnt(%u) uniq_cnt(%u)", 
                     non_null_cnt, input->uniq_cnt);
            return (false);
        }

        /* Tree count should remain unchanged */
        if (mkavl_count(input->tree_h) != input->uniq_cnt) {
            LOG_FAIL("unique check failed, mkavl_count(%u) uniq_cnt(%u)", 
                     mkavl_count(input->tree_h), input->uniq_cnt);
            return (false);
        }

        /* Put them all back in for the key */
        null_cnt = 0;
        for (j = 0; j < input->opts->node_cnt; ++j) {
            rc = mkavl_add_key_idx(input->tree_h, i, &(input->insert_seq[j]), 
                                   (void **) &existing_item);
            if (mkavl_rc_e_is_notok(rc)) {
                LOG_FAIL("add key idx failed, rc(%s)", 
                         mkavl_rc_e_get_string(rc));
                return (false);
            }

            if (NULL == existing_item) {
                ++null_cnt;
            }
        }

        if (null_cnt != input->uniq_cnt) {
            LOG_FAIL("unique check failed, null_cnt(%u) uniq_cnt(%u)", 
                     null_cnt, input->uniq_cnt);
            return (false);
        }

        /* Tree count should remain unchanged */
        if (mkavl_count(input->tree_h) != input->uniq_cnt) {
            LOG_FAIL("unique check failed, mkavl_count(%u) uniq_cnt(%u)", 
                     mkavl_count(input->tree_h), input->uniq_cnt);
            return (false);
        }
    }

    return (true);
}

static bool
mkavl_test_add_key_error (mkavl_test_input_st *input)
{
    uint32_t *existing_item;
    mkavl_rc_e rc;

    rc = mkavl_add_key_idx(NULL, MKAVL_TEST_KEY_E_ASC,
                           &(input->insert_seq[0]), (void **) &existing_item);
    if (mkavl_rc_e_is_ok(rc)) {
        LOG_FAIL("Key index operation failed, rc(%s)",
                 mkavl_rc_e_get_string(rc));
        return (false);
    }

    rc = mkavl_add_key_idx(input->tree_h, MKAVL_TEST_KEY_E_MAX,
                           &(input->insert_seq[0]), (void **) &existing_item);
    if (mkavl_rc_e_is_ok(rc)) {
        LOG_FAIL("Key index operation failed, rc(%s)",
                 mkavl_rc_e_get_string(rc));
        return (false);
    }

    rc = mkavl_add_key_idx(input->tree_h, MKAVL_TEST_KEY_E_ASC,
                           NULL, (void **) &existing_item);
    if (mkavl_rc_e_is_ok(rc)) {
        LOG_FAIL("Key index operation failed, rc(%s)",
                 mkavl_rc_e_get_string(rc));
        return (false);
    }

    rc = mkavl_add_key_idx(input->tree_h, MKAVL_TEST_KEY_E_ASC,
                           &(input->insert_seq[0]), NULL);
    if (mkavl_rc_e_is_ok(rc)) {
        LOG_FAIL("Key index operation failed, rc(%s)",
                 mkavl_rc_e_get_string(rc));
        return (false);
    }

    return (true);
}

static bool
mkavl_test_remove_key_error (mkavl_test_input_st *input)
{
    uint32_t *existing_item;
    mkavl_rc_e rc;

    rc = mkavl_remove_key_idx(NULL, MKAVL_TEST_KEY_E_ASC,
                           &(input->insert_seq[0]), (void **) &existing_item);
    if (mkavl_rc_e_is_ok(rc)) {
        LOG_FAIL("Key index operation failed, rc(%s)",
                 mkavl_rc_e_get_string(rc));
        return (false);
    }

    rc = mkavl_remove_key_idx(input->tree_h, MKAVL_TEST_KEY_E_MAX,
                           &(input->insert_seq[0]), (void **) &existing_item);
    if (mkavl_rc_e_is_ok(rc)) {
        LOG_FAIL("Key index operation failed, rc(%s)",
                 mkavl_rc_e_get_string(rc));
        return (false);
    }

    rc = mkavl_remove_key_idx(input->tree_h, MKAVL_TEST_KEY_E_ASC,
                           NULL, (void **) &existing_item);
    if (mkavl_rc_e_is_ok(rc)) {
        LOG_FAIL("Key index operation failed, rc(%s)",
                 mkavl_rc_e_get_string(rc));
        return (false);
    }

    rc = mkavl_remove_key_idx(input->tree_h, MKAVL_TEST_KEY_E_ASC,
                           &(input->insert_seq[0]), NULL);
    if (mkavl_rc_e_is_ok(rc)) {
        LOG_FAIL("Key index operation failed, rc(%s)",
                 mkavl_rc_e_get_string(rc));
        return (false);
    }

    return (true);
}

static bool
run_mkavl_test (mkavl_test_input_st *input)
{
    bool test_rc;

    if (NULL == input) {
        LOG_FAIL("invalid input");
        return (false);
    }

    test_rc = mkavl_test_new(input, NULL);
    if (!test_rc) {
        goto err_exit;
    }

    /* Destroy an empty tree */
    test_rc = mkavl_test_delete(input, NULL);
    if (!test_rc) {
        goto err_exit;
    }

    test_rc = mkavl_test_new(input, NULL);
    if (!test_rc) {
        goto err_exit;
    }

    /* Test new error input */
    test_rc = mkavl_test_new_error();
    if (!test_rc) {
        goto err_exit;
    }

    /* Add in all the items */
    test_rc = mkavl_test_add(input);
    if (!test_rc) {
        goto err_exit;
    }

    /* Test add error input */
    test_rc = mkavl_test_add_error(input);
    if (!test_rc) {
        goto err_exit;
    }

    /* Test finding equal items */
    test_rc = mkavl_test_find_equal(input);
    if (!test_rc) {
        goto err_exit;
    }

    /* Test all types of find */
    test_rc = mkavl_test_find_greater(input, false);
    if (!test_rc) {
        goto err_exit;
    }

    test_rc = mkavl_test_find_greater(input, true);
    if (!test_rc) {
        goto err_exit;
    }

    /* Test find error input */
    test_rc = mkavl_test_find_error(input);
    if (!test_rc) {
        goto err_exit;
    }

    /* Test find and add/remove from key */
    test_rc = mkavl_test_add_remove_key(input);
    if (!test_rc) {
        goto err_exit;
    }

    /* Test add/remove idx error conditions */
    test_rc = mkavl_test_add_key_error(input);
    if (!test_rc) {
        goto err_exit;
    }

    test_rc = mkavl_test_remove_key_error(input);
    if (!test_rc) {
        goto err_exit;
    }

    /* Copy tree: make sure copy fn is called as expected, test user defined
     * allocator here and in new */

    /* 
     * Iterate over both trees, make sure order is the same, everything is less
     * than in one iterator and greater than in the other.
     */

    /* Do walk over trees */

    /* Delete items from one tree */

    /* Destroy other tree: make sure destroy is called as expected */

    return (true);

err_exit:

    if (NULL != input->tree_h) {
        mkavl_test_delete(input, NULL);
    }

    return (false);
}
