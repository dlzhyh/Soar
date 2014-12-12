#include "portability.h"
/* -------------------------------------------------------------------
                              test.h

   Tests in conditions can be blank tests (null), tests for equality
   with a symbol, relational tests with a referent symbol, disjunctive
   tests between a list of constant symbols or a conjunction
   of multiple tests of any arbitrary type (except another conjunctive
   test).

   Note: This test structure and file was introduced in Soar 9.4 to
         support chunking of other symbol types and adding all test
         types to chunks.  The previous system differed significantly.
         - MMA 2013
------------------------------------------------------------------- */

#ifndef TEST_H_
#define TEST_H_

#include "kernel.h"

/* -- Forward declarations --- */
typedef struct node_varnames_struct node_varnames;
typedef struct condition_struct condition;
typedef struct wme_struct wme;
typedef struct rete_node_struct rete_node;
typedef struct rete_test_struct rete_test;
typedef char varnames;
typedef unsigned short rete_node_level;
typedef struct cons_struct cons;
typedef cons list;
typedef struct agent_struct agent;
typedef struct symbol_struct Symbol;
typedef signed short goal_stack_level;
typedef uint64_t tc_number;
template <typename T> inline void allocate_cons(agent* thisAgent, T* dest_cons_pointer);

/* identity_info is a struct used to hold the original symbol and grounding
 * information for a test.  It is used during chunking to determine which
 * constant symbols have the same semantics.
 *
 *    Type:             Test type in condition of original production matched
 *    symbol_type:      Symbol type in condition of original production matched
 *    grounding_id:     Unique numeric identifier of WME matched (a given WME
 *                      will have a different grounding ID for every level of
 *                      the goal stack.  Generated lazily.)
 *    grounding_index:  Which field of the WME (id, attr or value)
 *    grounding_wme:    This caches the matched wme when reconstructing the
 *                      condition when creating the instantiation.  It is needed
 *                      because we can't propagate the proper grounding IDs
 *                      until we know the match level of an instantiation.  So,
 *                      we temporarily cache the matched wme here while
 *                      reconstructing the conditions, and then later use that
 *                      wme to get IDs once we know the match level.
 *
 * Note: Conjunctive tests will not have symbol_type or grounding_id
 * or grounding index.*/

typedef struct identity_struct
{
    Symbol*        original_var;
    uint64_t      grounding_id;
    WME_Field     grounding_field;
    wme*           grounding_wme;
    identity_struct() : original_var(NULL), grounding_id(NON_GENERALIZABLE), grounding_field(NO_ELEMENT), grounding_wme(NULL) {}
} identity_info;

/* -- test_info stores information about a test.  If nil, the test is
 *    considered blank.
 *
 *    The original_test pointer stores the test that was defined when the
 *    production was read in by the parser.  The values are filled in by the
 *    rete when reconstructing a production.  It is used by the chunker to
 *    determine when to variablize non-STI symbols.
 *
 *    The eq_test pointer is used to cache the main equality test for an
 *    element in a condition so that we do not have to continually re-scan
 *
 *    Note that conjunctive tests always have a NULL original_test.  Each
 *    constituent test of the conjunctive test contains links to its original
 *    test already --*/

typedef struct test_struct
{
    TestType        type;                  /* see definitions in enums.h */
    union test_info_union
    {
        Symbol*        referent;         /* for relational tests */
        ::list*        disjunction_list;   /* for disjunction tests */
        ::list*        conjunct_list;      /* for conjunctive tests */
    } data;
    test_struct*     original_test;
    test_struct*     eq_test;
    identity_info*   identity;
    test_struct() : type(NUM_TEST_TYPES), original_test(NULL), eq_test(NULL), identity(NULL)
    {
        data.referent = NULL;
    }
} test_info;

/* --- Note that the test typedef is a *pointer* to a test struct. A test is
 *     considered blank when that pointer is nil. --- */
typedef test_info* test;


/* --- Descriptions of these functions can be found in the test.cpp --- */
inline bool test_is_blank(test t)
{
    return (t == 0);
}
inline bool test_is_variable(agent* thisAgent, test t);
inline bool test_has_referent(test t)
{
    return ((t->type != DISJUNCTION_TEST) && (t->type != GOAL_ID_TEST) &&
            (t->type != IMPASSE_ID_TEST) && (t->type != CONJUNCTIVE_TEST));
};
/* MToDo | Is this really necessary? */
inline test make_blank_test()
{
    return static_cast<test>(0);
}

char first_letter_from_test(test t);
bool tests_are_equal(test t1, test t2, bool neg);
bool tests_identical(test t1, test t2, bool considerIdentity = false);
bool test_includes_equality_test_for_symbol(test t, Symbol* sym);
bool test_includes_goal_or_impasse_id_test(test t, bool look_for_goal, bool look_for_impasse);
test copy_of_equality_test_found_in_test(agent* thisAgent, test t);
void cache_eq_test(test t);
test equality_test_found_in_test(test t);
test equality_var_test_found_in_test(test t);
test find_original_equality_test_preferring_vars(test t, bool useOriginals = false);

test make_test(agent* thisAgent, Symbol* sym, TestType test_type);
uint32_t hash_test(agent* thisAgent, test t);
void deallocate_test(agent* thisAgent, test t);

test copy_test(agent* thisAgent, test t);
test copy_test_removing_goal_impasse_tests(agent* thisAgent, test t, bool* removed_goal, bool* removed_impasse);
test copy_test_without_relationals(agent* thisAgent, test t);

void add_test(agent* thisAgent, test* dest_address, test new_test);
void add_test_if_not_already_there(agent* thisAgent, test* t, test new_test, bool neg);

::list* delete_test_from_conjunct(agent* thisAgent, test* t, ::list* pDeleteItem);

/* --- Some functions related to tests that used to be in rete.cpp */

void add_additional_tests_and_originals(agent* thisAgent, rete_node* node, condition* cond, wme* w, node_varnames* nvn, AddAdditionalTestsMode additional_tests);
void propagate_identity(agent* thisAgent, condition* cond, goal_stack_level level, bool use_negation_lookup = false);
void add_hash_info_to_id_test(agent* thisAgent, condition* cond, byte field_num, rete_node_level levels_up);
void add_hash_info_to_original_id_test(agent* thisAgent, condition* cond, byte field_num, rete_node_level levels_up);
void add_rete_test_list_to_tests(agent* thisAgent, condition* cond, rete_test* rt);
void add_gensymmed_equality_test(agent* thisAgent, test* t, char first_letter);
void add_all_variables_in_test(agent* thisAgent, test t, tc_number tc, list** var_list);
void add_bound_variables_in_test(agent* thisAgent, test t, tc_number tc, ::list** var_list);
void copy_non_identical_tests(agent* thisAgent, test* t, test add_me, bool considerIdentity = false);

/* UITODO| Make this method of Test */
const char* test_type_to_string(byte test_type);

#endif /* TEST_H_ */