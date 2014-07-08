/*************************************************************************
 * PLEASE SEE THE FILE "license.txt" (INCLUDED WITH THIS SOFTWARE PACKAGE)
 * FOR LICENSE AND COPYRIGHT INFORMATION.
 *************************************************************************/

/* instantiations.h */

#ifndef INSTANTIATIONS_H
#define INSTANTIATIONS_H

typedef struct condition_struct condition;
typedef struct symbol_struct Symbol;
typedef struct wme_struct wme;
typedef struct preference_struct preference;
typedef signed short goal_stack_level;
typedef uint64_t tc_number;


/* -------------------------------------------------------------------
                        Instantiations and Nots

   Instantiations record three main things:
     (1) the instantiated LHS of the production,
     (2) any "<>" tests that are between identifiers and that occur in
         top-level positive conditions on the LHS, and
     (3) the still-existing preferences that were generated by the RHS.

   Fields in an instantiation:

      prod:  points to the production.  (Note: this can also be NIL, for
        fake instantiations used for goal ^item augmentations.  See decide.c.)

      next, prev:  used for a doubly-linked list of instantiations of this
        production that are still in the match set.

      rete_token, rete_wme:  these fields are reserved for use by the Rete.
        (The Rete needs them to find the proper instantiation to retract
        when a token is delted from a p_node.)

      top_of_instantiated_conditions, bottom_of_instantiated_conditions:
        point to the top and bottom of the instantiated LHS conditions.

      nots:  header of a singly-linked list of Nots from the LHS.

      preferences_generated:  header for a doubly-linked list of existing
        preferences that were created by this instantiation.

      match_goal:  points to the match goal of the instantiation, or NIL
        if there is none.

      match_goal_level:  goal stack level of the match goal, or
        ATTRIBUTE_IMPASSE_LEVEL if there is no match goal.

      reliable:  false iff instantiation is a justification whose
        backtrace either:

        - tests ^quiescence t, or
        - contains a local negated condition and learn -N is set, or
        - goes through an unreliable justification

        Intuitively, a justification is unreliable if its creation is
        not guaranteed by the state of production and working memory

      in_ms:  true iff this instantiation is still in the match set (i.e.,
        Rete-supported).
      backtrace_number:  used by the chunker to avoid backtracing through
        the same instantiation twice during the building of the same chunk.

      GDS_evaluated_already:  Most productions produce several actions.
        When we compute the goal-dependency-set (gds) gds for one wme of an
        instantiation, there's no point in redoing the work for a second wme
        from the same instantiation since the gds will be the same.  By
        testing this flag, we avoid duplicating this work.  The value is set
        to false whenever an instantiation is created.

   Reference counts on instantiations:
      +1 if it's in the match set
      +1 for each preference it created that's still around
   The reference count is kept implicitly using the preferences_generated
   and in_ms fields.  We deallocate an instantiation if its reference count
   goes to 0.
------------------------------------------------------------------- */



struct not_struct {
  struct not_struct *next;  /* next Not in the singly-linked list */
  Symbol *s1;               /* the two identifiers constrained to be "<>" */
  Symbol *s2;
};

typedef struct instantiation_struct {
  struct production_struct *prod; /* used full name of struct because
								     a forward declaration is needed -ajc (5/1/02) */
  struct instantiation_struct *next, *prev; /* dll of inst's from same prod */
  struct token_struct *rete_token;       /* used by Rete for retractions */
  wme *rete_wme;                         /* ditto */
  condition *top_of_instantiated_conditions;
  condition *bottom_of_instantiated_conditions;
  not_struct *nots;
  preference *preferences_generated;    /* header for dll of prefs */
  Symbol *match_goal;                   /* symbol, or NIL if none */
  goal_stack_level match_goal_level;    /* level, or ATTRIBUTE_IMPASSE_LEVEL */
  bool reliable;
  bool in_ms;  /* true iff this inst. is still in the match set */
  tc_number backtrace_number;
  bool GDS_evaluated_already;
} instantiation;

/* REW: begin 09.15.96 */
/* A dll of instantiations that will be used to determine the gds through
   a backtracing-style procedure, evaluate_gds in decide.cpp */

typedef struct pi_struct {
  struct pi_struct *next, *prev;
  instantiation *inst;
} parent_inst;
/* REW: end   09.15.96 */

#endif
