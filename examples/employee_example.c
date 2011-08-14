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

#include "examples_common.h"
#include <math.h>

/** The default employee count for runs */
static const uint32_t default_employee_cnt = 1000;
/** The default number of test runs */
static const uint32_t default_run_cnt = 1;
/** The default verbosity level of messages displayed */
static const uint8_t default_verbosity = 0;

/**
 * Upper bound on name string lengths.
 */
#define MAX_NAME_LEN 100

/**
 * State for the current test execution.
 */
typedef struct employee_example_opts_st_ {
    /** The number of employees in the DB */
    uint32_t employee_cnt;
    /** The number of separate runs to do */
    uint32_t run_cnt;
    /** The RNG seed for the first run */
    uint32_t seed;
    /** The verbosity level for the test */
    uint8_t verbosity;
} employee_example_opts_st;

/** List of first names to choose from employees */
static const char *first_names[] = {
    "Jacob", "Isabella", "Ethan", "Sophia", "Michael", "Emma", "Jayden",
    "Olivia", "William", "Ava", "Alexander", "Emily", "Noah", "Abigail",
    "Daniel", "Madison", "Aiden", "Chloe", "Anthony", "Mia", "Joshua",
    "Addison", "Mason", "Elizabeth", "Christopher", "Ella", "Andrew", "Natalie",
    "David", "Samantha", "Matthew", "Alexis", "Logan", "Lily", "Elijah",
    "Grace", "James", "Hailey", "Joseph", "Alyssa", "Gabriel", "Lillian",
    "Benjamin", "Hannah", "Ryan", "Avery", "Samuel", "Leah", "Jackson",
    "Nevaeh", "John", "Sofia", "Nathan", "Ashley", "Jonathan", "Anna",
    "Christian", "Brianna", "Liam", "Sarah", "Dylan", "Zoe", "Landon",
    "Victoria", "Caleb", "Gabriella", "Tyler", "Brooklyn", "Lucas", "Kaylee",
    "Evan", "Taylor", "Gavin", "Layla", "Nicholas", "Allison", "Isaac",
    "Evelyn", "Brayden", "Riley", "Luke", "Amelia", "Angel", "Khloe", "Brandon",
    "Makayla", "Jack", "Aubrey", "Isaiah", "Charlotte", "Jordan", "Savannah",
    "Owen", "Zoey", "Carter", "Bella", "Connor", "Kayla", "Justin", "Alexa"
};

/** List of last names to choose from employees */
static const char *last_names[] = {
    "Smith", "Johnson", "Williams", "Jones", "Brown", "Davis", "Miller",
    "Wilson", "Moore", "Taylor", "Anderson", "Thomas", "Jackson", "White",
    "Harris", "Martin", "Thompson", "Garcia", "Martinez", "Robinson", "Clark",
    "Rodriguez", "Lewis", "Lee", "Walker", "Hall", "Allen", "Young",
    "Hernandez", "King", "Wright", "Lopez", "Hill", "Scott", "Green", "Adams",
    "Baker", "Gonzalez", "Nelson", "Carter", "Mitchell", "Perez", "Roberts",
    "Turner", "Phillips", "Campbell", "Parker", "Evans", "Edwards", "Collins",
    "Stewart", "Sanchez", "Morris", "Rogers", "Reed", "Cook", "Morgan", "Bell",
    "Murphy", "Bailey", "Rivera", "Cooper", "Richardson", "Cox", "Howard",
    "Ward", "Torres", "Peterson", "Gray", "Ramirez", "James", "Watson",
    "Brooks", "Kelly", "Sanders", "Price", "Bennett", "Wood", "Barnes", "Ross",
    "Henderson", "Coleman", "Jenkins", "Perry", "Powell", "Long", "Patterson",
    "Hughes", "Flores", "Washington", "Butler", "Simmons", "Foster", "Gonzales",
    "Bryant", "Alexander", "Russell", "Griffin", "Diaz", "Hayes"
};

typedef struct employee_obj_st_ {
    uint32_t id;
    char first_name[MAX_NAME_LEN];
    char last_name[MAX_NAME_LEN];
} employee_obj_st;

/**
 * The input structure to pass test parameters to functions.
 */
typedef struct employee_example_input_st_ {
    /** The input options for the run */
    const employee_example_opts_st *opts;
    /** The tree for the run */
    mkavl_tree_handle tree_h;
} employee_example_input_st;


typedef struct employee_ctx_st_ {
    uint32_t nodes_walked;
    uint32_t match_cnt;
} employee_ctx_st;

typedef struct employee_walk_ctx_st_ {
    char lookup_last_name[MAX_NAME_LEN];
} employee_walk_ctx_st;

/**
 * The context storted for a tree.
 */
typedef struct mkavl_test_ctx_st_ {
    /** A sanity check field */
    uint32_t magic;
    /** A count for how many times mkavl_test_copy_fn() was called */
    uint32_t copy_cnt;
    /** A count for how many times mkavl_test_item_fn() was called */
    uint32_t item_fn_cnt;
    /** A count for how many time mkavl_test_copy_malloc() was called */ 
    uint32_t copy_malloc_cnt;
    /** A count for how many time mkavl_test_copy_free() was called */ 
    uint32_t copy_free_cnt;
} mkavl_test_ctx_st;

/**
 * Display the program's help screen and exit as needed.
 *
 * @param do_exit Whether to exit after the output.
 * @param exit_val If exiting the value with which to exit.
 */
static void
print_usage (bool do_exit, int32_t exit_val)
{
    printf("\nExample of using mkavl for an employee DB\n\n");
    printf("Usage:\n");
    printf("-s <seed>\n"
           "   The starting seed for the RNG (default=seeded by time()).\n");
    printf("-n <employees>\n"
           "   The number of nodes to place in the trees (default=%u).\n",
           default_employee_cnt);
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

/**
 * Utility function to output the value of the options.
 *
 * @param opts The options to output.
 */
static void
print_opts (employee_example_opts_st *opts)
{
    if (NULL == opts) {
        return;
    }

    printf("employee_example_opts: seed=%u, employee_cnt=%u, run_cnt=%u,\n"
           "                       verbosity=%u\n",
           opts->seed, opts->employee_cnt, opts->run_cnt, opts->verbosity);
}

/**
 * Store the command line options into a local structure.
 *
 * @param argc The number of options
 * @param argv The string for the options.
 * @param opts The local structure in which to store the parsed info.
 */
static void
parse_command_line (int argc, char **argv, employee_example_opts_st *opts)
{
    int c;
    char *end_ptr;
    uint32_t val;

    if (NULL == opts) {
        return;
    }

    opts->employee_cnt = default_employee_cnt;
    opts->run_cnt = default_run_cnt;
    opts->verbosity = default_verbosity;
    opts->seed = (uint32_t) time(NULL);

    while ((c = getopt(argc, argv, "n:r:v:s:h")) != -1) {
        switch (c) {
        case 'n':
            val = strtol(optarg, &end_ptr, 10);
            if ((end_ptr != optarg) && (0 == errno)) {
                opts->employee_cnt = val;
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

    if (0 == opts->employee_cnt) {
        printf("Error: employee count(%u) must be non-zero\n",
               opts->employee_cnt);
        print_usage(true, EXIT_SUCCESS);
    }

    if (opts->verbosity >= 3) {
        print_opts(opts);
    }
}

/**
 * Compare employees by ID.
 *
 * @param item1 Item to compare
 * @param item2 Item to compare
 * @param context Context for the tree
 * @return Comparison result
 */
static int32_t
employee_cmp_by_id (const void *item1, const void *item2, void *context)
{
    const employee_obj_st *e1 = item1;
    const employee_obj_st *e2 = item2;

    if ((NULL == e1) || (NULL == e2)) {
        abort();
    }

    if (e1->id < e2->id) {
        return (-1);
    } else if (e1->id > e2->id) {
        return (1);
    }

    return (0);
}

/**
 * Compare employees by last name and ID.
 *
 * @param item1 Item to compare
 * @param item2 Item to compare
 * @param context Context for the tree
 * @return Comparison result
 */
static int32_t
employee_cmp_by_last_name (const void *item1, const void *item2, void *context)
{
    const employee_obj_st *e1 = item1;
    const employee_obj_st *e2 = item2;
    employee_ctx_st *ctx = (employee_ctx_st *) context;
    int32_t str_rc;

    if ((NULL == e1) || (NULL == e2) || (NULL == ctx)) {
        abort();
    }

    ++(ctx->nodes_walked);

    /* 
     * Compare by last name first.  This ensures last names are grouped
     * together.
     */ 
    str_rc = strncmp(e1->last_name, e2->last_name, sizeof(e1->last_name));
    if (0 != str_rc) {
        return (str_rc);
    }

    /* If the last name is the same, compare by employee ID which is unique */
    if (e1->id < e2->id) {
        return (-1);
    } else if (e1->id > e2->id) {
        return (1);
    }

    return (0);
}

/**
 * The values for the key ordering.
 */
typedef enum employee_example_key_e_ {
    /** Ordered by ID */
    EMPLOYEE_EXAMPLE_KEY_E_ID,
    /** Ordered by last name + ID */
    EMPLOYEE_EXAMPLE_KEY_E_LNAME_ID,
    /** Max value for boundary testing */
    EMPLOYEE_EXAMPLE_KEY_E_MAX,
} employee_example_key_e;

/** The comparison functions to use */
static mkavl_compare_fn cmp_fn_array[] = { 
    employee_cmp_by_id, 
    employee_cmp_by_last_name 
};

/** @cond doxygen_suppress */
CT_ASSERT(NELEMS(cmp_fn_array) == EMPLOYEE_EXAMPLE_KEY_E_MAX);
/** @endcond */

static bool
generate_employee (employee_obj_st **obj)
{
    static uint32_t next_id = 1;
    employee_obj_st *local_obj;
    uint32_t first_name_idx, last_name_idx;

    if (NULL == obj) {
        return (false);
    }
    *obj = NULL;

    local_obj = calloc(1, sizeof(*local_obj));
    if (NULL == local_obj) {
        return (false);
    }

    local_obj->id = next_id++;
    first_name_idx = (rand() % NELEMS(first_names));
    last_name_idx = (rand() % NELEMS(last_names));
    my_strlcpy(local_obj->first_name, first_names[first_name_idx],
               sizeof(local_obj->first_name));
    my_strlcpy(local_obj->last_name, last_names[last_name_idx],
               sizeof(local_obj->last_name));

    *obj = local_obj;

    return (true);
}

static void
display_employee (employee_obj_st *obj)
{
    if (NULL == obj) {
        return;
    }

    printf("Employee(ID=%u, Name=\"%s %s\")\n", obj->id, obj->first_name,
           obj->last_name);
}

static mkavl_rc_e
free_employee (void *item, void *context)
{
    if (NULL == item) {
        return (MKAVL_RC_E_EINVAL);
    }

    free(item);

    return (MKAVL_RC_E_SUCCESS);
}

static void
lookup_employees_by_last_name (employee_example_input_st *input,
                               const char *last_name, uint32_t max_records,
                               bool find_all, bool do_print)
{
    employee_obj_st *found_item;
    employee_obj_st lookup_item = {0};
    uint32_t num_records = 0;
    mkavl_rc_e rc;
    employee_ctx_st *ctx = mkavl_get_tree_context(input->tree_h);

    assert_abort(NULL != ctx);

    /* Set ID to the minimum possible value */
    lookup_item.id = 0;
    my_strlcpy(lookup_item.last_name, last_name, sizeof(lookup_item.last_name));

    rc = mkavl_find(input->tree_h, MKAVL_FIND_TYPE_E_GE,
                    EMPLOYEE_EXAMPLE_KEY_E_LNAME_ID, &lookup_item,
                    (void **) &found_item);
    assert_abort(mkavl_rc_e_is_ok(rc));

    while ((NULL != found_item) &&
           (0 == strncmp(last_name, found_item->last_name,
                         sizeof(found_item->last_name))) &&
           (find_all || (num_records < max_records))) {

        if (do_print) {
            printf("%2u. ", (num_records + 1));
            display_employee(found_item);
        }

        rc = mkavl_find(input->tree_h, MKAVL_FIND_TYPE_E_GT,
                        EMPLOYEE_EXAMPLE_KEY_E_LNAME_ID, found_item,
                        (void **) &found_item);
        assert_abort(mkavl_rc_e_is_ok(rc));
        ++num_records;
    }

    ctx->match_cnt = num_records;
}

static mkavl_rc_e
last_name_walk_cb (void *item, void *tree_context, void *walk_context,
                   bool *stop_walk)
{
    employee_obj_st *e = item;
    employee_ctx_st *ctx = (employee_ctx_st *) tree_context;
    employee_walk_ctx_st *walk_ctx = (employee_walk_ctx_st *) walk_context;

    if ((NULL == e) || (NULL == ctx) || (NULL == walk_ctx) ||
        (NULL == stop_walk)) {
        return (MKAVL_RC_E_EINVAL);
    }
    *stop_walk = false;

    ++(ctx->nodes_walked);
    if (0 == strncmp(e->last_name, walk_ctx->lookup_last_name,
                     sizeof(e->last_name))) {
        ++(ctx->match_cnt);
    }

    return (MKAVL_RC_E_SUCCESS);
}

static void
run_employee_example (employee_example_input_st *input)
{
    employee_obj_st *cur_item, *found_item;
    employee_obj_st lookup_item = {0};
    const uint32_t lookup_cnt = 10;
    const char *last_name_lookups[20];
    uint32_t match_cnt_array[NELEMS(last_name_lookups)] = {0};
    mkavl_rc_e mkavl_rc;
    bool bool_rc;
    uint32_t i, idx;
    uint32_t lookup_id;
    const char *new_last_name;
    char old_last_name[MAX_NAME_LEN];
    struct timeval tv;
    double t1, t2;
    double key_lookup_time, nonkey_lookup_time;
    uint32_t key_nodes_walked, nonkey_nodes_walked;
    employee_ctx_st ctx = {0};
    employee_walk_ctx_st walk_ctx = {{0}};

    printf("\n");

    mkavl_rc = mkavl_new(&(input->tree_h), cmp_fn_array, NELEMS(cmp_fn_array),
                         &ctx, NULL);
    assert_abort(mkavl_rc_e_is_ok(mkavl_rc));

    for (i = 0; i < input->opts->employee_cnt; ++i) {
        bool_rc = generate_employee(&cur_item);
        assert_abort((NULL != cur_item) && bool_rc);

        if (input->opts->verbosity >= 3) {
            printf("Adding employee to DB:\n   ");
            display_employee(cur_item);
        }

        mkavl_rc = mkavl_add(input->tree_h, cur_item,
                             (void **) &found_item);
        assert_abort((NULL == found_item) && 
                     mkavl_rc_e_is_ok(mkavl_rc));
    }

    printf("*** Testing functionality ***\n\n");

    printf("Find %u employees by ID\n", lookup_cnt);
    for (i = 0; i < 10; ++i) {
        lookup_id = (1 + (rand() % input->opts->employee_cnt));
        lookup_item.id = lookup_id;
        mkavl_rc = mkavl_find(input->tree_h, MKAVL_FIND_TYPE_E_EQUAL, 
                              EMPLOYEE_EXAMPLE_KEY_E_ID,
                              &lookup_item,
                              (void **) &found_item);
        assert_abort((NULL != found_item) && 
                     mkavl_rc_e_is_ok(mkavl_rc));
        printf("Looking up ID %u: ", lookup_id);
        display_employee(found_item);
    }
    printf("\n");

    idx = (rand() % NELEMS(last_names));
    printf("Finding up to first %u employees with last name %s\n",
           lookup_cnt, last_names[idx]);
    lookup_employees_by_last_name(input, last_names[idx], lookup_cnt,
                                  false, true);
    
    printf("\n");

    lookup_id = (1 + (rand() % input->opts->employee_cnt));
    lookup_item.id = lookup_id;
    mkavl_rc = mkavl_find(input->tree_h, MKAVL_FIND_TYPE_E_EQUAL, 
                          EMPLOYEE_EXAMPLE_KEY_E_ID,
                          &lookup_item,
                          (void **) &found_item);
    assert_abort((NULL != found_item) && 
                 mkavl_rc_e_is_ok(mkavl_rc));
    cur_item = found_item;

    idx = (rand() % NELEMS(last_names));
    new_last_name = last_names[idx];
    my_strlcpy(old_last_name, cur_item->last_name, sizeof(old_last_name));

    printf("Changing last name of %s %s (ID=%u) to %s\n",
           cur_item->first_name, cur_item->last_name, cur_item->id,
           new_last_name);

    mkavl_rc = mkavl_remove_key_idx(input->tree_h,
                                    EMPLOYEE_EXAMPLE_KEY_E_LNAME_ID, cur_item,
                                    (void **) &found_item);
    assert_abort((NULL != found_item) && 
                 mkavl_rc_e_is_ok(mkavl_rc));
    cur_item = found_item;

    my_strlcpy(cur_item->last_name, new_last_name, sizeof(cur_item->last_name));

    mkavl_rc = mkavl_add_key_idx(input->tree_h, EMPLOYEE_EXAMPLE_KEY_E_LNAME_ID,
                                 cur_item, (void **) &found_item);
    assert_abort((NULL == found_item) && 
                 mkavl_rc_e_is_ok(mkavl_rc));

    lookup_item.id = cur_item->id;
    mkavl_rc = mkavl_find(input->tree_h, MKAVL_FIND_TYPE_E_EQUAL, 
                          EMPLOYEE_EXAMPLE_KEY_E_ID,
                          &lookup_item,
                          (void **) &found_item);
    assert_abort((NULL != found_item) && 
                 mkavl_rc_e_is_ok(mkavl_rc));
    printf("Lookup for ID %u: ", lookup_item.id);
    display_employee(found_item);

    lookup_item.id = cur_item->id;
    my_strlcpy(lookup_item.last_name, cur_item->last_name,
               sizeof(lookup_item.last_name));
    mkavl_rc = mkavl_find(input->tree_h, MKAVL_FIND_TYPE_E_EQUAL, 
                          EMPLOYEE_EXAMPLE_KEY_E_LNAME_ID,
                          &lookup_item,
                          (void **) &found_item);
    assert_abort((NULL != found_item) && 
                 mkavl_rc_e_is_ok(mkavl_rc));
    printf("Lookup for last name \"%s\", ID %u:\n   ", lookup_item.last_name,
           lookup_item.id);
    display_employee(found_item);

    my_strlcpy(lookup_item.last_name, old_last_name,
               sizeof(lookup_item.last_name));
    mkavl_rc = mkavl_find(input->tree_h, MKAVL_FIND_TYPE_E_EQUAL, 
                          EMPLOYEE_EXAMPLE_KEY_E_LNAME_ID,
                          &lookup_item,
                          (void **) &found_item);
    assert_abort(mkavl_rc_e_is_ok(mkavl_rc));
    printf("Lookup for last name \"%s\", ID %u: ", old_last_name,
           lookup_item.id);
    if (NULL == found_item) {
        printf("not found");
    } else { 
        display_employee(found_item);
    }
    printf("\n");

    printf("\n");

    printf("*** Testing performance ***\n\n");

    // TODO: pareto or gaussian distribution for last names.

    /* Fill in last names to lookup */
    for (i = 0; i < NELEMS(last_name_lookups); ++i) {
        idx = (rand() % NELEMS(last_names));
        last_name_lookups[i] = last_names[idx];
    }

    /* Test keyed lookup */
    gettimeofday(&tv, NULL);
    t1 = timeval_to_seconds(&tv);
    ctx.nodes_walked = 0;

    for (i = 0; i < NELEMS(last_name_lookups); ++i) {
        lookup_employees_by_last_name(input, last_name_lookups[i], 0,
                                      true, false);
        match_cnt_array[i] = ctx.match_cnt;
    }

    gettimeofday(&tv, NULL);
    t2 = timeval_to_seconds(&tv);

    key_lookup_time = (t2 - t1);
    key_nodes_walked = ctx.nodes_walked;

    /* Test non-keyed lookup */
    gettimeofday(&tv, NULL);
    t1 = timeval_to_seconds(&tv);
    ctx.nodes_walked = 0;

    for (i = 0; i < NELEMS(last_name_lookups); ++i) {
        ctx.match_cnt = 0;
        my_strlcpy(walk_ctx.lookup_last_name, last_name_lookups[i],
                   sizeof(walk_ctx.lookup_last_name));
        mkavl_rc = mkavl_walk(input->tree_h, last_name_walk_cb, &walk_ctx);
        assert_abort(mkavl_rc_e_is_ok(mkavl_rc));
        if (match_cnt_array[i] != ctx.match_cnt) {
            printf("ERROR: for name %s, keyed lookup found %u matches and "
                   "non-key lookup found %u matches\n",
                   last_name_lookups[i], match_cnt_array[i], ctx.match_cnt);

        }
    }

    gettimeofday(&tv, NULL);
    t2 = timeval_to_seconds(&tv);

    nonkey_lookup_time = (t2 - t1);
    nonkey_nodes_walked = ctx.nodes_walked;

    printf("Keyed lookup time: %.6lfs, Non-keyed lookup time: %.6lfs, "
           "Ratio: %.2lf\n", key_lookup_time, nonkey_lookup_time,
           (key_lookup_time / nonkey_lookup_time));
    printf("Keyed nodes compared: %u, Non-keyed nodes walked: %u, "
           "Ratio: %.2lf\n", key_nodes_walked, nonkey_nodes_walked,
           ((double) key_nodes_walked / nonkey_nodes_walked));
    
    mkavl_rc = mkavl_delete(&(input->tree_h), free_employee, NULL);
    assert_abort(mkavl_rc_e_is_ok(mkavl_rc));

    printf("\n");
}

/**
 * Main function to test objects.
 */
int 
main (int argc, char *argv[])
{
    employee_example_opts_st opts;
    uint32_t cur_run, cur_seed;
    employee_example_input_st input = {0};

    parse_command_line(argc, argv, &opts);

    /*
    {
        srand(opts.seed);
        double r, u, h, l, a;
        uint32_t i;
        h = 101.0;
        l = 1.0;
        a = 0.5;
        for (i = 0; i < 500; ++i) {
            u = ((double) rand() / RAND_MAX);
            r = pow((pow(l, a) / (u*pow((l/h), a) - u + 1)), (1.0/a));
            //r=(1.0 * pow(1- ((double) rand() / RAND_MAX) , (-1/3.0)));
            if (floor(r) > 98) {
                printf("r=%lf\n", r);
            }
        }
        exit(EXIT_SUCCESS);
    }
    */

    printf("\n");
    cur_seed = opts.seed;
    for (cur_run = 0; cur_run < opts.run_cnt; ++cur_run) {

        printf("Doing run %u with seed %u\n", (cur_run + 1), cur_seed);
        srand(cur_seed);

        input.opts = &opts;
        input.tree_h = NULL;

        run_employee_example(&input);

        ++cur_seed;
    }

    printf("\n");

    return (0);
}
