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

static uint32_t
get_unique_count (uint32_t *array, size_t num_elem)
{
    uint32_t temp_array[num_elem];
    uint32_t count = 0;
    uint32_t cur_val;
    uint32_t i;
    size_t size = (num_elem * sizeof(*array));

    if ((NULL == array) || (0 == num_elem)) {
        return (0);
    }

    memcpy(temp_array, array, size);
    qsort(temp_array, num_elem, sizeof(*array), uint32_t_cmp);

    for (i = 0; i < num_elem; ++i) {
        if ((0 == i) || (temp_array[i] != cur_val)) {
            cur_val = temp_array[i];
            ++count;
        }
    }

    return (count);
}

/* 
 * Do a forward declaration so we can keep all the AVL setup stuff separate
 * below main().
 */
static bool
run_mkavl_test(uint32_t *insert_seq, uint32_t *delete_seq, 
               uint32_t uniq_cnt, test_mkavl_opts_st *opts);
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

    parse_command_line(argc, argv, &opts);

    cur_seed = opts.seed;
    for (cur_run = 0; cur_run < opts.run_cnt; ++cur_run) {
        uint32_t insert_seq[opts.node_cnt];
        uint32_t delete_seq[opts.node_cnt];

        printf("Doing run %u with seed %u\n", (cur_run + 1), cur_seed);
        srand(cur_seed);

        for (i = 0; i < opts.node_cnt; ++i) {
            insert_seq[i] = ((rand() % opts.range_end) + opts.range_start);
        }
        permute_array(insert_seq, delete_seq, opts.node_cnt);
        uniq_cnt = get_unique_count(insert_seq, opts.node_cnt);

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

        was_success = run_mkavl_test(insert_seq, delete_seq, uniq_cnt, &opts);
        if (!was_success) {
            printf("FAILURE: the test has failed for seed %u!!!\n", cur_seed);
            ++fail_count;
        }

        ++cur_seed;
    }

    if (0 != fail_count) {
        printf("\n%u TEST(S) FAILED\n", fail_count);
    }

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

static bool
run_mkavl_test (uint32_t *insert_seq, uint32_t *delete_seq, 
                uint32_t uniq_cnt, test_mkavl_opts_st *opts)
{
    mkavl_tree_handle tree_h = NULL;
    mkavl_rc_e rc = MKAVL_RC_E_SUCCESS;
    mkavl_compare_fn cmp_fn_array[] = { mkavl_cmp_fn1 , mkavl_cmp_fn2 };
    uint32_t dup_cnt, null_cnt, non_null_cnt;;
    uint32_t i;
    uint32_t *existing_item;

    if ((NULL == insert_seq) || (NULL == delete_seq) ||
        (NULL == opts)) {
        LOG_FAIL("invalid input");
        return (false);
    }
    dup_cnt = (opts->node_cnt - uniq_cnt);

    rc = mkavl_new(&tree_h, cmp_fn_array, NELEMS(cmp_fn_array),
                   (void *) TEST_MKAVL_MAGIC, NULL);
    if (mkavl_rc_e_is_notok(rc)) {
        LOG_FAIL("new failed, rc(%s)", mkavl_rc_e_get_string(rc));
        goto err_exit;
    }

    /* Destroy an empty tree */
    rc = mkavl_delete(&tree_h, NULL);
    if (mkavl_rc_e_is_notok(rc)) {
        LOG_FAIL("delete empty failed, rc(%s)", mkavl_rc_e_get_string(rc));
        goto err_exit;
    }

    rc = mkavl_new(&tree_h, cmp_fn_array, NELEMS(cmp_fn_array),
                   (void *) TEST_MKAVL_MAGIC, NULL);
    if (mkavl_rc_e_is_notok(rc)) {
        LOG_FAIL("new failed, rc(%s)", mkavl_rc_e_get_string(rc));
        goto err_exit;
    }

    /* Add in all the items */
    non_null_cnt = 0;
    for (i = 0; i < opts->node_cnt; ++i) {
        rc = mkavl_add(tree_h, &(insert_seq[i]), (void **) &existing_item);
        if (mkavl_rc_e_is_notok(rc)) {
            LOG_FAIL("add failed, rc(%s)", mkavl_rc_e_get_string(rc));
            goto err_exit;
        }

        if (NULL != existing_item) {
            ++non_null_cnt;
        }
    }

    if (non_null_cnt != dup_cnt) {
        LOG_FAIL("duplidate check failed, non_null_cnt(%u) dup_cnt(%u)", 
                 non_null_cnt, dup_cnt);
        goto err_exit;
    }

    if (mkavl_count(tree_h) != uniq_cnt) {
        LOG_FAIL("unique check failed, mkavl_count(%u) uniq_cnt(%u)", 
                 mkavl_count(tree_h), uniq_cnt);
        goto err_exit;
    }

    null_cnt = 0;

    /* Test find and add/remove from thread */

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

    if (NULL != tree_h) {
        rc = mkavl_delete(&tree_h, NULL);
        if (mkavl_rc_e_is_notok(rc)) {
            LOG_FAIL("delete failed, rc(%s)", mkavl_rc_e_get_string(rc));
        }
    }

    return (false);
}
