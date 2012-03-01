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
 * This is a basic example of how the mkavl library can be used for memory
 * management.  The free and allocated memory blocks are maintained in a single
 * mvavl DB.  The DB is indexed by the starting address of the memory block as
 * one key and the other key consists of the allocation status (i.e., free or
 * allocated), block size, and starting address.
 *
 * On a malloc() call, we look up the free block with the size greater than or
 * equal to the requested size.  This is a O(lg N) best-fit algorithm.  On a
 * free() call, we change the state of the freed block from allocated to free.
 * We then check whether the adjacent memory blocks are also free and, if so,
 * consolidate the blocks into one.
 *
 * The example run will:
 *    -# Allocate 100 pointers
 *    -# Free up to half of them.
 *    -# Re-allocate the ones just freed.
 *    -# Free all the pointers.
 *
 * At each step, we print out a graphical display of the curent memory state.
 * The step where up to half are freed is done by chosing points uniformly at
 * random by default.  A command  line option allows you to instead free the
 * first half of the pointers deterministically.
 *
 * Of course, this is just an example so we use malloc to generate the AVL items
 * placed in the tree.  In reality, a more complicated scheme would be
 * implemented to grab memory for the purpose so malloc isn't being called to
 * implement malloc.
 *
 * \verbatim
    Example of using mkavl for an memory allocation

    Usage:
    -s <seed>
       The starting seed for the RNG (default=seeded by time()).
    -b <memory size in bytes>
       The number of bytes in memory (default=409600).
    -n <number of allocations>
       The max number of allocations at any one time (default=100).
    -r <runs>
       The number of runs to do (default=1).
    -l
       Free/re-allocate linearly (default=uniform distribution).
    -v <verbosity level>
       A higher number gives more output (default=0).
    -h
       Display this help message.
   \endverbatim
 */

#include "examples_common.h"

/** List of sizes for memory allocations */
static const size_t malloc_sizes[] = { 4, 8, 512, 4096 };

/** The default number of items to allocated at any one time */
static const uint32_t default_malloc_cnt = 100;
/** The default memory size */
static uint32_t default_memory_size;
/** The default number of test runs */
static const uint32_t default_run_cnt = 1;
/** The default verbosity level of messages displayed */
static const uint8_t default_verbosity = 0;

/** The address to use as the base of the memory */
static void *base_addr = (void *) 0x1234ABCD;
/** Maximum size of the memory */
static size_t max_memory_size;

/**
 * Patterns for how memory gets freed and re-allocated.
 */
typedef enum malloc_pattern_e_ {
    /** Free and re-allocate the first N memory locations */
    MALLOC_PATTERN_E_LINEAR,
    /** 
     * Free and re-allocated the memory in locations chosen from a uniform
     * distribution.
     */
    MALLOC_PATTERN_E_UNIFORM,
    /** Max value for boundary checking */
    MALLOC_PATTERN_E_MAX,
} malloc_pattern_e;

/**
 * State for the current test execution.
 */
typedef struct malloc_example_opts_st_ {
    /** The max number of allocations at any given time */
    uint32_t malloc_cnt;
    /** The size of the memory */
    size_t memory_size;
    /** The number of separate runs to do */
    uint32_t run_cnt;
    /** The RNG seed for the first run */
    uint32_t seed;
    /** The verbosity level for the test */
    uint8_t verbosity;
    /** The allocation pattern to use */
    malloc_pattern_e pattern;
} malloc_example_opts_st;

/**
 * The data for a free/allocated memory block.
 */
typedef struct memblock_obj_st_ {
    /** 
     * Starting address for the memory (this is obviously unique for each memory
     * block).
     */
    void *start_addr;
    /** The byte count for how big the block is */
    size_t byte_cnt;
    /** Whether the block is allocated or free */
    bool is_allocated;
} memblock_obj_st;

/**
 * The input structure to pass test parameters to functions.
 */
typedef struct malloc_example_input_st_ {
    /** The input options for the run */
    const malloc_example_opts_st *opts;
    /** The tree for the run */
    mkavl_tree_handle tree_h;
} malloc_example_input_st;

/**
 * The context associated with the memblock AVLs.
 */
typedef struct memblock_ctx_st_ {
    /** Counter for the number of nodes walked for a given test */
    uint32_t nodes_walked;
} memblock_ctx_st;

/**
 * Display the program's help screen and exit as needed.
 *
 * @param do_exit Whether to exit after the output.
 * @param exit_val If exiting the value with which to exit.
 */
static void
print_usage (bool do_exit, int32_t exit_val)
{
    printf("\nExample of using mkavl for an memory allocation\n\n");
    printf("Usage:\n");
    printf("-s <seed>\n"
           "   The starting seed for the RNG (default=seeded by time()).\n");
    printf("-b <memory size in bytes>\n"
           "   The number of bytes in memory (default=%u).\n",
           default_memory_size);
    printf("-n <number of allocations>\n"
           "   The max number of allocations at any one time (default=%u).\n",
           default_malloc_cnt);
    printf("-r <runs>\n"
           "   The number of runs to do (default=%u).\n",
           default_run_cnt);
    printf("-l\n"
           "   Free/re-allocate linearly (default=uniform distribution).\n");
    printf("-v <verbosity level>\n"
           "   A higher number gives more output (default=%u).\n",
           default_verbosity);
    printf("-h\n"
           "   Display this help message.\n");
    printf("\n");

    if (do_exit) {
        exit(exit_val);
    }
}

/**
 * Utility function to output the value of the options.
 *
 * @param opts The options to output.
 */
static void
print_opts (malloc_example_opts_st *opts)
{
    if (NULL == opts) {
        return;
    }

    printf("malloc_example_opts: seed=%u, malloc_cnt=%u, run_cnt=%u,\n"
           "                     verbosity=%u, memory_size=%zu\n"
           "                     pattern=%u\n",
           opts->seed, opts->malloc_cnt, opts->run_cnt, opts->verbosity,
           opts->memory_size, opts->pattern);
}

/**
 * Store the command line options into a local structure.
 *
 * @param argc The number of options
 * @param argv The string for the options.
 * @param opts The local structure in which to store the parsed info.
 */
static void
parse_command_line (int argc, char **argv, malloc_example_opts_st *opts)
{
    int c;
    char *end_ptr;
    uint32_t val;

    if (NULL == opts) {
        return;
    }

    opts->malloc_cnt = default_malloc_cnt;
    opts->memory_size = default_memory_size;
    opts->run_cnt = default_run_cnt;
    opts->verbosity = default_verbosity;
    opts->pattern = MALLOC_PATTERN_E_UNIFORM;
    opts->seed = (uint32_t) time(NULL);

    while ((c = getopt(argc, argv, "n:r:v:s:hb:l")) != -1) {
        switch (c) {
        case 'n':
            val = strtol(optarg, &end_ptr, 10);
            if ((end_ptr != optarg) && (0 == errno)) {
                opts->malloc_cnt = val;
            }
            break;
        case 'b':
            val = strtol(optarg, &end_ptr, 10);
            if ((end_ptr != optarg) && (0 == errno)) {
                opts->memory_size = val;
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
        case 'l':
            opts->pattern = MALLOC_PATTERN_E_LINEAR;
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

    if (0 == opts->malloc_cnt) {
        printf("Error: malloc count(%u) must be non-zero\n",
               opts->malloc_cnt);
        print_usage(true, EXIT_SUCCESS);
    }

    if (opts->memory_size > max_memory_size) {
        printf("Error: memory size(%zu) must be no greater than %zu\n",
               opts->memory_size, max_memory_size);
        print_usage(true, EXIT_SUCCESS);
    }

    if (opts->verbosity >= 3) {
        print_opts(opts);
    }
}

/**
 * Compare memory blocks by address.
 *
 * @param item1 Item to compare
 * @param item2 Item to compare
 * @param context Context for the tree
 * @return Comparison result
 */
static int32_t
memblock_cmp_by_addr (const void *item1, const void *item2, void *context)
{
    const memblock_obj_st *m1 = item1;
    const memblock_obj_st *m2 = item2;

    if ((NULL == m1) || (NULL == m2)) {
        abort();
    }

    if (m1->start_addr < m2->start_addr) {
        return (-1);
    } else if (m1->start_addr > m2->start_addr) {
        return (1);
    }

    return (0);
}

/**
 * Compare memory blocks by allocated status, size, and address.
 *
 * @param item1 Item to compare
 * @param item2 Item to compare
 * @param context Context for the tree
 * @return Comparison result
 */
static int32_t
memblock_cmp_by_size (const void *item1, const void *item2, void *context)
{
    const memblock_obj_st *m1 = item1;
    const memblock_obj_st *m2 = item2;

    if ((NULL == m1) || (NULL == m2)) {
        abort();
    }

    /* First, group by allocation status */
    if (!m1->is_allocated && m2->is_allocated) {
        return (-1);
    } else if (m1->is_allocated && !m2->is_allocated) {
        return (1);
    }

    /* Then by size */
    if (m1->byte_cnt < m2->byte_cnt) {
        return (-1);
    } else if (m1->byte_cnt > m2->byte_cnt) {
        return (1);
    }

    /* Finally by address, which is guaranteed unique */
    if (m1->start_addr < m2->start_addr) {
        return (-1);
    } else if (m1->start_addr > m2->start_addr) {
        return (1);
    }

    return (0);
}

/**
 * The values for the key ordering.
 */
typedef enum malloc_example_key_e_ {
    /** Ordered by address */
    MALLOC_EXAMPLE_KEY_E_ADDR,
    /** Ordered by allocation status + size + address */
    MALLOC_EXAMPLE_KEY_E_SIZE,
    /** Max value for boundary testing */
    MALLOC_EXAMPLE_KEY_E_MAX,
} malloc_example_key_e;

/** The comparison functions to use */
static mkavl_compare_fn cmp_fn_array[] = { 
    memblock_cmp_by_addr, 
    memblock_cmp_by_size 
};

/** @cond doxygen_suppress */
CT_ASSERT(NELEMS(cmp_fn_array) == MALLOC_EXAMPLE_KEY_E_MAX);
/** @endcond */

/**
 * Callback to free the given memory block object.
 *
 * @param item The pointer to the object.
 * @param context Context for the tree.
 * @return The return code
 */
static mkavl_rc_e
free_memblock (void *item, void *context)
{
    if (NULL == item) {
        return (MKAVL_RC_E_EINVAL);
    }

    free(item);

    return (MKAVL_RC_E_SUCCESS);
}

/**
 * Display memory in the given range.
 *
 * @param tree_h The memory block trees.
 * @param start_addr The start address of the range.
 * @param bytes The number of bytes to display.
 */
static void
display_memory (mkavl_tree_handle tree_h, void *start_addr, size_t bytes)
{
    memblock_obj_st *found_item, *cur_item;
    memblock_obj_st lookup_item = {0};
    uint32_t loop_cnt = 0;
    void *end_addr = (start_addr + bytes);
    mkavl_rc_e rc;

    assert_abort(NULL != tree_h);

    lookup_item.start_addr = start_addr;
    rc = mkavl_find(tree_h, MKAVL_FIND_TYPE_E_GE,
                    MALLOC_EXAMPLE_KEY_E_ADDR, &lookup_item,
                    (void **) &found_item);
    assert_abort(mkavl_rc_e_is_ok(rc));
    cur_item = found_item;

    printf("\n*** Displaying memory from %p to %p (size=%zu) ***\n",
           start_addr, end_addr, bytes);
    printf("XXX = allocated, OOO = free\n\n");

    while ((NULL != cur_item) &&
           (cur_item->start_addr <= end_addr)) {
        assert_abort(loop_cnt < EXAMPLES_RUNAWAY_SANITY);

        printf("   %p: %s (%zu bytes)\n", cur_item->start_addr,
               (cur_item->is_allocated) ? "XXXXXX" : "OOOOOO",
               cur_item->byte_cnt);

        rc = mkavl_find(tree_h, MKAVL_FIND_TYPE_E_GT,
                        MALLOC_EXAMPLE_KEY_E_ADDR, cur_item,
                        (void **) &found_item);
        assert_abort(mkavl_rc_e_is_ok(rc));
        cur_item = found_item;
        ++loop_cnt;
    }

    printf("\n*** Finished displaying memory ***\n\n");
}

/**
 * Allocate and fill in the data for a memory block object.  By default, the
 * object is set to not allocated.
 *
 * Obviously in a real implementation, malloc() wouldn't be used to implement
 * malloc.  You'd need to slice up the memory available yourself.
 *
 * @param obj A pointer to fill in with the newly allocated object.
 * @param start_addr The start address for the block.
 * @param byte_cnt The byte count for the block.
 * @return true if the creation was successful.
 */
static bool
generate_memblock (memblock_obj_st **obj, void *start_addr, 
                   size_t byte_cnt)
{
    memblock_obj_st *local_obj;

    if (NULL == obj) {
        return (false);
    }
    *obj = NULL;

    local_obj = calloc(1, sizeof(*local_obj));
    if (NULL == local_obj) {
        return (false);
    }

    local_obj->start_addr = start_addr;
    local_obj->byte_cnt = byte_cnt;
    local_obj->is_allocated = false;

    *obj = local_obj;

    return (true);
}

/**
 * This is a best-fit version that will find the first unallocated memory block
 * large enough to hold the request.
 *
 * @param tree_h The tree of memory blocks.
 * @param size The size of memory to allocate.
 * @return A pointer to the memory of NULL if the allocation failed.
 */
static void *
my_malloc (mkavl_tree_handle tree_h, size_t size)
{
    memblock_obj_st lookup_item = {0};
    memblock_obj_st *cur_item, *found_item, *new_item;
    size_t new_size;
    void *new_addr;
    bool bool_rc;
    mkavl_rc_e rc;

    if ((NULL == tree_h) || (0 == size)) {
        return (NULL);
    }

    /* Set to minimum possible value */
    lookup_item.start_addr = base_addr;
    lookup_item.byte_cnt = size;
    lookup_item.is_allocated = false;

    rc = mkavl_find(tree_h, MKAVL_FIND_TYPE_E_GE,
                    MALLOC_EXAMPLE_KEY_E_SIZE, &lookup_item,
                    (void **) &found_item);
    assert_abort(mkavl_rc_e_is_ok(rc));

    if (NULL == found_item) {
        /* Can't satisfy request */
        return (NULL);
    }

    cur_item = found_item;

    rc = mkavl_remove_key_idx(tree_h, MALLOC_EXAMPLE_KEY_E_SIZE, 
                              cur_item, (void **) &found_item);
    assert_abort((NULL != found_item) && 
                 mkavl_rc_e_is_ok(rc));
    cur_item = found_item;

    cur_item->is_allocated = true;

    /* Split the memory block in two if larger than necessary */
    if (cur_item->byte_cnt > size) {

        new_addr = (cur_item->start_addr + size);
        new_size = (cur_item->byte_cnt - size);
        cur_item->byte_cnt = size;

        bool_rc = generate_memblock(&new_item, new_addr, new_size);
        assert_abort((NULL != new_item) && bool_rc);

        rc = mkavl_add(tree_h, new_item, (void **) &found_item);
        assert_abort((NULL == found_item) && mkavl_rc_e_is_ok(rc));
    }

    rc = mkavl_add_key_idx(tree_h, MALLOC_EXAMPLE_KEY_E_SIZE,
                           cur_item, (void **) &found_item);
    assert_abort((NULL == found_item) && 
                 mkavl_rc_e_is_ok(rc));

    return (cur_item->start_addr);
}

/**
 * Mark the memory as unallocated and merge with adjacent unallocated blocks.
 *
 * @param tree_h The tree of memory blocks.
 * @param ptr The pointer to free.
 */
static void
my_free (mkavl_tree_handle tree_h, void *ptr)
{
    memblock_obj_st *found_item, *cur_item, *prev_item, *next_item;
    memblock_obj_st *update_item = NULL;
    memblock_obj_st lookup_item = {0};
    size_t new_size;
    mkavl_rc_e rc;

    if ((NULL == tree_h) || (NULL == ptr)) {
        return;
    }

    lookup_item.start_addr = ptr;
    rc = mkavl_find(tree_h, MKAVL_FIND_TYPE_E_EQUAL,
                    MALLOC_EXAMPLE_KEY_E_ADDR, &lookup_item,
                    (void **) &found_item);
    assert_abort(mkavl_rc_e_is_ok(rc) &&
                 (NULL != found_item) &&
                 (found_item->is_allocated));
    cur_item = found_item;
    update_item = cur_item;
    new_size = cur_item->byte_cnt;

    rc = mkavl_find(tree_h, MKAVL_FIND_TYPE_E_GT,
                    MALLOC_EXAMPLE_KEY_E_ADDR, &lookup_item,
                    (void **) &found_item);
    assert_abort(mkavl_rc_e_is_ok(rc));
    next_item = found_item;

    if ((NULL != next_item) && !next_item->is_allocated) {
        rc = mkavl_remove(tree_h, next_item, (void **) &found_item);
        assert_abort((NULL != found_item) && 
                     mkavl_rc_e_is_ok(rc));
        new_size += next_item->byte_cnt;
        free(next_item);
        next_item = NULL;
    }

    rc = mkavl_find(tree_h, MKAVL_FIND_TYPE_E_LT,
                    MALLOC_EXAMPLE_KEY_E_ADDR, &lookup_item,
                    (void **) &found_item);
    assert_abort(mkavl_rc_e_is_ok(rc));
    prev_item = found_item;

    if ((NULL != prev_item) && !prev_item->is_allocated) {
        rc = mkavl_remove(tree_h, cur_item, (void **) &found_item);
        assert_abort((NULL != found_item) && 
                     mkavl_rc_e_is_ok(rc));
        update_item = prev_item;
        new_size += prev_item->byte_cnt;
        free(cur_item);
        cur_item = NULL;
    }

    rc = mkavl_remove_key_idx(tree_h, MALLOC_EXAMPLE_KEY_E_SIZE, 
                              update_item, (void **) &found_item);
    assert_abort((NULL != found_item) && 
                 mkavl_rc_e_is_ok(rc));
    update_item = found_item;

    update_item->byte_cnt = new_size;
    update_item->is_allocated = false;

    rc = mkavl_add_key_idx(tree_h, MALLOC_EXAMPLE_KEY_E_SIZE,
                           update_item, (void **) &found_item);
    assert_abort((NULL == found_item) && 
                 mkavl_rc_e_is_ok(rc));
}

/**
 * Run a single instance of an example.
 *
 * @param input The input parameters for the run.
 */
static void
run_malloc_example (malloc_example_input_st *input)
{
    memblock_obj_st *cur_item, *found_item;
    bool bool_rc;
    memblock_ctx_st ctx = {0};
    uint32_t i, size_idx, idx, cnt;
    void *ptr_array[input->opts->malloc_cnt];
    mkavl_rc_e mkavl_rc;

    printf("\n");

    mkavl_rc = mkavl_new(&(input->tree_h), cmp_fn_array, NELEMS(cmp_fn_array),
                         &ctx, NULL);
    assert_abort(mkavl_rc_e_is_ok(mkavl_rc));

    /* Create the entire block of memory to use */
    bool_rc = generate_memblock(&cur_item, base_addr, input->opts->memory_size);
    assert_abort((NULL != cur_item) && bool_rc);

    mkavl_rc = mkavl_add(input->tree_h, cur_item, (void **) &found_item);
    assert_abort((NULL == found_item) && mkavl_rc_e_is_ok(mkavl_rc));

    printf("Created memory\n");
    display_memory(input->tree_h, base_addr, input->opts->memory_size);

    /* Allocate all the pointers */
    for (i = 0; i < input->opts->malloc_cnt; ++i) {
        size_idx = (rand() % NELEMS(malloc_sizes));
        ptr_array[i] = my_malloc(input->tree_h, malloc_sizes[size_idx]);
        assert_abort(NULL != ptr_array[i]);
    }
    
    printf("Allocated %u pointers\n", input->opts->malloc_cnt);
    display_memory(input->tree_h, base_addr, input->opts->memory_size);

    /* Free up to half the pointers */
    cnt = 0;
    for (i = 0; i < (input->opts->malloc_cnt / 2); ++i) {
        switch (input->opts->pattern) {
        case MALLOC_PATTERN_E_LINEAR:
            idx = i;
            break;
        case MALLOC_PATTERN_E_UNIFORM:
            idx = (rand() % NELEMS(ptr_array));
            break;
        default:
            abort();
            break;
        }
        if (NULL != ptr_array[idx]) {
            my_free(input->tree_h, ptr_array[idx]);
            ptr_array[idx] = NULL;
            ++cnt;
        }
    }

    printf("Freed %u pointers\n", cnt);
    display_memory(input->tree_h, base_addr, input->opts->memory_size);

    /* Re-allocate those pointers */
    cnt = 0;
    for (i = 0; i < input->opts->malloc_cnt; ++i) {
        if (NULL != ptr_array[i]) {
            continue;
        }
        size_idx = (rand() % NELEMS(malloc_sizes));
        ptr_array[i] = my_malloc(input->tree_h, malloc_sizes[size_idx]);
        assert_abort(NULL != ptr_array[i]);
        ++cnt;
    }

    printf("Allocated %u pointers\n", cnt);
    display_memory(input->tree_h, base_addr, input->opts->memory_size);

    /* Free all the pointers */
    for (i = 0; i < input->opts->malloc_cnt; ++i) {
        my_free(input->tree_h, ptr_array[i]);
        ptr_array[i] = NULL;
    }

    printf("Freed all memory\n");
    display_memory(input->tree_h, base_addr, input->opts->memory_size);

    mkavl_rc = mkavl_delete(&(input->tree_h), free_memblock, NULL);
    assert_abort(mkavl_rc_e_is_ok(mkavl_rc));

    printf("\n");
}

/**
 * Main function to test objects.
 */
int 
main (int argc, char *argv[])
{
    malloc_example_opts_st opts;
    uint32_t cur_run, cur_seed;
    malloc_example_input_st input = {0};

    default_memory_size = (4096 * default_malloc_cnt);
    max_memory_size = (UINTPTR_MAX - (uintptr_t) base_addr);

    parse_command_line(argc, argv, &opts);

    printf("\n");
    cur_seed = opts.seed;
    for (cur_run = 0; cur_run < opts.run_cnt; ++cur_run) {

        printf("Doing run %u with seed %u\n", (cur_run + 1), cur_seed);
        srand(cur_seed);

        input.opts = &opts;
        input.tree_h = NULL;

        run_malloc_example(&input);

        ++cur_seed;
    }

    printf("\n");

    return (0);
}
