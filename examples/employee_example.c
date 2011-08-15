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
 * This is an example of how the mkavl library can be used.  This example
 * consists of a DB of employees where their unique ID and first and last name
 * is stored.  The first names of employees are chosen uniformly at random from
 * a list of 100 popular names.  The last names of employees are, by default,
 * chosen uniformly at random from a list of 100 common names.  There is a
 * command-line option to use a 
 * <a href="http://en.wikipedia.org/wiki/Zipf_distribution">Zipf 
 * distribution</a> instead for the last name to
 * give significantly more weight towards choosing the most popular names.
 *
 * Running the example gives two phases: functionality and performance.
 *
 * For functionality:
 *    -# Choose ten IDs uniformly at random and lookup the employee objects.
 *    -# Choose a last name uniformly at random and lookup up to the first ten
 *    employees with that last name.  Note that this is done in <i>O(lg N)</i>
 *    time.
 *    -# Change the last name of an employee and show that all the lookups
 *    happen as expected.
 *
 * For performance:
 *    -#  Choose 30 last names uniformly at random.  Lookup all the employees
 *    with each last name using the mkavl tree keyed by last name and ID
 *    (<i>O(lg N)</i>).  Then, lookup all the employees with each last name in
 *    the tree by walking through all nodes (as would typically be done for a
 *    non-key field) (<i>O(N)</i>).
 *    -# Compare the wall clock time (i.e., from gettimeofday()) for both lookup
 *    methods and the total number of nodes walked for each method.
 *
 * Performance results show for 1000 employees and 100 last names, the keyed
 * lookup shows an improvement by a factor of 10 for uniformly distributed last
 * names.  For a Zipf distribution, a majority of the runs show about a factor
 * of 20 improvement.  However, some Zipf runs only show an improvement of about
 * 3 (presumably when some of the most popular names are randomly chosen and
 * there are just inherently a lot of employees to lookup for that name).
 *
 * \verbatim
   Example of using mkavl for an employee DB
   
   Usage:
   -s <seed>
      The starting seed for the RNG (default=seeded by time()).
   -n <employees>
      The number of nodes to place in the trees (default=1000).
   -r <runs>
      The number of runs to do (default=1).
   -v <verbosity level>
      A higher number gives more output (default=0).
   -z
      Use Zipf distribution for last names (default=uniform).
   -a <Zipf alpha>
      If using a Zipf distribution, the alpha value to    
      parameterize the distribution (default=1.000000).
   -h
      Display this help message.
   \endverbatim
 */

#include "examples_common.h"
#include <math.h>

/**
 * Probability distributions used within example.
 */
typedef enum employee_dist_e_ {
    /** Uniform distribution */
    EMPLOYEE_DIST_E_UNIFORM,
    /** Zipf distribution */
    EMPLOYEE_DIST_E_ZIPF,
    /** Max value for boundary checking */
    EMPLOYEE_DIST_E_MAX,
} employee_dist_e;

/** The default employee count for runs */
static const uint32_t default_employee_cnt = 1000;
/** The default number of test runs */
static const uint32_t default_run_cnt = 1;
/** The default verbosity level of messages displayed */
static const uint8_t default_verbosity = 0;
/** The default last name distribution function */
static const employee_dist_e default_last_name_dist = EMPLOYEE_DIST_E_UNIFORM;
/** The default alpha parameter for the Zipf distribution */
static const double default_zipf_alpha = 1.0;

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
    /** The distribution function to use for last names */
    employee_dist_e last_name_dist;
    /** The alpha value to parameterize a Zipf distribution */
    double zipf_alpha;
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

/**
 * The data stored for employees.
 */
typedef struct employee_obj_st_ {
    /** Unique ID for the employee */
    uint32_t id;
    /** First name */
    char first_name[MAX_NAME_LEN];
    /** Last name */
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

/**
 * The context associated with the employee AVLs.
 */
typedef struct employee_ctx_st_ {
    /** Counter for the number of nodes walked for a given test */
    uint32_t nodes_walked;
    /** Counter for the number of matches found for a given test */
    uint32_t match_cnt;
} employee_ctx_st;

/**
 * Context for the walk of the employee AVLs.
 */
typedef struct employee_walk_ctx_st_ {
    /** Last name for which the walk is being done */
    char lookup_last_name[MAX_NAME_LEN];
} employee_walk_ctx_st;

/**
 * Get a random variable from a Zipf distribution within the range [1,n].
 * Implementation is from:
 * http://www.cse.usf.edu/~christen/tools/genzipf.c
 *
 * @param alpha The alpha paremeter for the distribution.
 * @param n The maximum value (inclusive) for the random variable.
 * @return A value between 1 and n (inclusive).
 */
static uint32_t
zipf (double alpha, uint32_t n)
{
    static uint32_t cached_n = 0;
    static double cached_c = 0.0;
    double z;
    double sum_prob;
    double zipf_value;
    double c = 0.0;
    uint32_t i;

    if (n != cached_n) {
        for (i = 1; i <= n; ++i) {
            c = (c + (1.0 / pow((double) i, alpha)));
        }
        cached_c = (1.0 / c);
        cached_n = n;
    }
    c = cached_c;

    /* Insert industrial strength RNG method here */
    z = ((double) rand() / RAND_MAX);

    /* Map z to the value */
    sum_prob = 0;
    for (i = 1; i <= n; i++) {
        sum_prob = sum_prob + (c / pow((double) i, alpha));
        if (sum_prob >= z) {
            zipf_value = i;
            break;
        }
    }

    /* Equations below are for Pareto and bounded Pareto distributions */
    //r=(m * pow((1- z), (-1/a)));
    //r = pow((pow(l, a) / (z*pow((l/h), a) - z + 1)), (1.0/a));

    /* Assert that zipf_value is between 1 and N */
   assert_abort((zipf_value >= 1) && (zipf_value <= n));

    return (zipf_value);
}

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
    printf("-z\n"
           "   Use Zipf distribution for last names (default=uniform).\n");
    printf("-a <Zipf alpha>\n"
           "   If using a Zipf distribution, the alpha value to\n"
           "   parameterize the distribution (default=%lf).\n",
           default_zipf_alpha);
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
print_opts (employee_example_opts_st *opts)
{
    if (NULL == opts) {
        return;
    }

    printf("employee_example_opts: seed=%u, employee_cnt=%u, run_cnt=%u,\n"
           "                       verbosity=%u last_name_dist=%u\n"
           "                       zipf_alpha=%lf\n",
           opts->seed, opts->employee_cnt, opts->run_cnt, opts->verbosity,
           opts->last_name_dist, opts->zipf_alpha);
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
    double dval;

    if (NULL == opts) {
        return;
    }

    opts->employee_cnt = default_employee_cnt;
    opts->run_cnt = default_run_cnt;
    opts->verbosity = default_verbosity;
    opts->last_name_dist = default_last_name_dist;
    opts->zipf_alpha = default_zipf_alpha;
    opts->seed = (uint32_t) time(NULL);

    while ((c = getopt(argc, argv, "n:r:v:s:hza:")) != -1) {
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
        case 'a':
            dval = strtod(optarg, &end_ptr);
            if ((end_ptr != optarg) && (0 == errno)) {
                opts->zipf_alpha = dval;
            }
            break;
        case 'z':
            opts->last_name_dist = EMPLOYEE_DIST_E_ZIPF;
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

    if (!(opts->zipf_alpha > 0.0)) {
        printf("Error: Zipf alpha(%lf) must be greater than 0.0\n",
               opts->zipf_alpha);
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

/**
 * Allocate and fill in the data for an employee object.  The ID of the employee
 * is a unique value for the employee.  The first name is chosen from a uniform
 * distribution of 100 names.  The last name is chosen according to the given
 * distribution of 100 names.
 *
 * @param input The input parameters.
 * @param obj A pointer to fill in with the newly allocated object.
 * @return true if the creation was successful.
 */
static bool
generate_employee (employee_example_input_st *input, employee_obj_st **obj)
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
    switch (input->opts->last_name_dist) {
    case EMPLOYEE_DIST_E_UNIFORM:
        last_name_idx = (rand() % NELEMS(last_names));
        break;
    case EMPLOYEE_DIST_E_ZIPF:
        last_name_idx = (zipf(input->opts->zipf_alpha, NELEMS(last_names)) - 1);
        break;
    default:
        abort();
        break;
    }
    my_strlcpy(local_obj->first_name, first_names[first_name_idx],
               sizeof(local_obj->first_name));
    my_strlcpy(local_obj->last_name, last_names[last_name_idx],
               sizeof(local_obj->last_name));

    *obj = local_obj;

    return (true);
}

/**
 * Display the given employee object.
 *
 * @param obj The object to display.
 */
static void
display_employee (employee_obj_st *obj)
{
    if (NULL == obj) {
        return;
    }

    printf("Employee(ID=%u, Name=\"%s %s\")\n", obj->id, obj->first_name,
           obj->last_name);
}

/**
 * Callback to free the given employee object.
 *
 * @param item The pointer to the object.
 * @param context Context for the tree.
 * @return The return code
 */
static mkavl_rc_e
free_employee (void *item, void *context)
{
    if (NULL == item) {
        return (MKAVL_RC_E_EINVAL);
    }

    free(item);

    return (MKAVL_RC_E_SUCCESS);
}

/**
 * Look up a (sub)set of employees by their last name.
 *
 * @param input The input parameters for the run.
 * @param last_name The last name for which to search.
 * @param max_records The maximum number of records to find (if find_all is
 * false).
 * @param find_all If true, then find all employee records for the given last
 * name (max_records is ignored in this case).
 * @param do_print Whether to print the info for each record.
 */
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

/**
 * Callback for walking the tree to find all records for a given last name.
 *
 * @param item The current employee object.
 * @param tree_context The context for the tree.
 * @param walk_context The context for the walk.
 * @param stop_walk Setting to true will stop the walk immediately upon return.
 * @return The return code.
 */
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

/**
 * Run a single instance of an example.
 *
 * @param input The input parameters for the run.
 */
static void
run_employee_example (employee_example_input_st *input)
{
    employee_obj_st *cur_item, *found_item;
    employee_obj_st lookup_item = {0};
    const uint32_t lookup_cnt = 10;
    const char *last_name_lookups[30];
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
        bool_rc = generate_employee(input, &cur_item);
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
