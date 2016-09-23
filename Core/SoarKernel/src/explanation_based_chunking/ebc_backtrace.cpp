/*************************************************************************
 *
 *  file:  backtrace.cpp
 *
 * =======================================================================
 *  Backtracing structures and routines.  See also explain.c
 * =======================================================================
 */

/* ====================================================================
                        Backtracing routines
   ==================================================================== */

#include "ebc.h"

#include "agent.h"
#include "condition.h"
#include "dprint.h"
#include "explanation_memory.h"
#include "instantiation.h"
#include "mem.h"
#include "memory_manager.h"
#include "output_manager.h"
#include "preference.h"
#include "print.h"
#include "production.h"
#include "instantiation.h"
#include "soar_TraceNames.h"
#include "symbol.h"
#include "symbol_manager.h"
#include "test.h"
#include "working_memory.h"
#include "xml.h"

#include <stdlib.h>

using namespace soar_TraceNames;

/* ====================================================================

                            Backtracing

   Four sets of conditions are maintained during backtracing:  locals,
   grounds, positive potentials, and negateds.  Negateds are really
   potentials, but we keep them separately throughout backtracing, and
   ground them at the very end.  Note that this means during backtracing,
   the grounds, positive potentials, and locals are all instantiated
   top-level positive conditions, so they all have a bt.wme_ on them.

   In order to avoid backtracing through the same instantiation twice,
   we mark each instantiation as we BT it, by setting
   inst->backtrace_number = backtrace_number (this is a global variable
   which gets incremented each time we build a chunk).

   The add_to_grounds() and add_to_locals()
   macros below are used to add conditions to these sets.  The negated
   conditions are maintained in the chunk_cond_set "negated_set."

==================================================================== */

void Explanation_Based_Chunker::add_to_grounds(condition* cond)
{
    if ((cond)->bt.wme_->grounds_tc != grounds_tc)
    {
        (cond)->bt.wme_->grounds_tc = grounds_tc;
        cond->bt.wme_->chunker_bt_last_ground_cond = cond;
    }
    if (cond->bt.wme_->chunker_bt_last_ground_cond != cond)
    {
        add_singleton_unification_if_needed(cond);
    }
    push(thisAgent, (cond), grounds);
    dprint(DT_BACKTRACE, "--> Ground condition added: %l.\n", cond);
}

void Explanation_Based_Chunker::add_to_locals(condition* cond)
{
    add_local_singleton_unification_if_needed(cond);
    push(thisAgent, (cond), locals);
    dprint(DT_BACKTRACE, "--> Local condition added: %l.\n", cond);
}

/* -------------------------------------------------------------------
                     Backtrace Through Instantiation

   This routine BT's through a given instantiation.  The general method
   is as follows:

     1. If we've already BT'd this instantiation, then skip it.
     2. Mark the TC (in the instantiated conditions) of all higher goal
        ids tested in top-level positive conditions
     3. Scan through the instantiated conditions; add each one to the
        appropriate set (locals, grounds, negated_set).
------------------------------------------------------------------- */

/* mvp 5-17-94 */
void print_consed_list_of_conditions(agent* thisAgent, list* c, int indent)
{
    for (; c != NIL; c = c->rest)
    {
        if (thisAgent->outputManager->get_printer_output_column(thisAgent) >= COLUMNS_PER_LINE - 20)
        {
            thisAgent->outputManager->printa_sf(thisAgent,  "\n      ");
        }

        /* mvp 5-17-94 */
        thisAgent->outputManager->print_spaces(thisAgent, indent);
        print_condition(thisAgent, static_cast<condition_struct*>(c->first));
    }
}

/* mvp 5-17-94 */
void print_consed_list_of_condition_wmes(agent* thisAgent, list* c, int indent)
{
    for (; c != NIL; c = c->rest)
    {
        if (thisAgent->outputManager->get_printer_output_column(thisAgent) >= COLUMNS_PER_LINE - 20)
        {
            thisAgent->outputManager->printa_sf(thisAgent,  "\n      ");
        }

        /* mvp 5-17-94 */
        thisAgent->outputManager->print_spaces(thisAgent, indent);
        thisAgent->outputManager->printa_sf(thisAgent,  "     ");
        print_wme(thisAgent, (static_cast<condition*>(c->first))->bt.wme_);
    }
}

/* This is the wme which is causing this production to be backtraced through.
   It is NULL when backtracing for a result preference.                   */

inline bool condition_is_operational(condition* cond, goal_stack_level grounds_level)
{
    Symbol* thisID = cond->data.tests.id_test->eq_test->data.referent;
     
    assert(thisID->id->is_sti());
        assert(thisID->id->level <= cond->bt.level);

    return  (thisID->id->level <= grounds_level);
}

void Explanation_Based_Chunker::backtrace_through_instantiation(instantiation* inst,
                                     goal_stack_level grounds_level,
                                     condition* trace_cond,
                                     const identity_triple o_ids_to_replace,
                                     const rhs_triple rhs_funcs,
                                     uint64_t bt_depth,
                                     BTSourceType bt_type)
{

    condition* c;
    list* grounds_to_print, *locals_to_print, *negateds_to_print;

    dprint(DT_BACKTRACE, "Backtracing %y :i%u (matched level %d):\n", inst->prod_name, inst->i_id, static_cast<int64_t>(grounds_level));
//    dprint(DT_BACKTRACE, "           RHS identities: (%y [o%u] ^%y [o%u] %y [o%u]),\n           Matched cond: %l\n",
//        get_ovar_for_o_id(o_ids_to_replace.id),o_ids_to_replace.id,
//        get_ovar_for_o_id(o_ids_to_replace.attr),o_ids_to_replace.attr,
//        get_ovar_for_o_id(o_ids_to_replace.value), o_ids_to_replace.value, trace_cond);
    if (thisAgent->sysparams[TRACE_BACKTRACING_SYSPARAM])
    {
        thisAgent->outputManager->printa_sf(thisAgent,  "... BT through instantiation of ");
        if (inst->prod)
        {
            thisAgent->outputManager->printa_sf(thisAgent, "%y\n", inst->prod_name);
        }
        else
        {
            thisAgent->outputManager->printa(thisAgent, "[Architectural Fake Instantiation]\n");
        }

        xml_begin_tag(thisAgent, kTagBacktrace);
        if (inst->prod)
        {
            xml_att_val(thisAgent, kProduction_Name, inst->prod_name);
        }
        else
        {
            xml_att_val(thisAgent, kProduction_Name, "[Architectural Fake Instantiation]");
        }

    }

    if (trace_cond)
    {
        unify_backtraced_conditions(trace_cond, o_ids_to_replace, rhs_funcs);
    }

    ++bt_depth;
    if (inst->explain_depth > bt_depth)
    {
        inst->explain_depth = bt_depth;
    }
    /* --- if the instantiation has already been BT'd, don't repeat it --- */
    if (inst->backtrace_number == backtrace_number)
    {
        if (thisAgent->sysparams[TRACE_BACKTRACING_SYSPARAM])
        {

            /* mvp 5-17-94 */
            thisAgent->outputManager->printa(thisAgent, "(We already backtraced through this instantiation.)\n");
            xml_att_val(thisAgent, kBacktracedAlready, "true");
            xml_end_tag(thisAgent, kTagBacktrace);
        }
        #ifdef BUILD_WITH_EXPLAINER
        thisAgent->explanationMemory->increment_stat_seen_instantations_backtraced();
        #endif
        dprint(DT_BACKTRACE, "... already backtraced through.\n");
        return;
    }

    inst->backtrace_number = backtrace_number;
    #ifdef BUILD_WITH_EXPLAINER
    thisAgent->explanationMemory->add_bt_instantiation(inst, bt_type);
    thisAgent->explanationMemory->increment_stat_instantations_backtraced();
    #endif
    if (!inst->reliable)
    {
        m_reliable = false;
    }

    Symbol* thisID, *value;

    for (c = inst->top_of_instantiated_conditions; c != NIL; c = c->next)
    {
        if (c->type == POSITIVE_CONDITION)
        {
            cache_constraints_in_cond(c);
            if (condition_is_operational(c, grounds_level))
            {
                if (c->bt.wme_->grounds_tc != grounds_tc)   /* First time we've seen something matching this wme*/
                {
                    add_to_grounds(c);
                }
                else                                        /* Another condition that matches the same wme */
                {
                    add_to_grounds(c);
                    add_singleton_unification_if_needed(c);
                }
            } else {
                add_to_locals(c);
            }
        }
        else
        {
            dprint(DT_BACKTRACE, "Backtracing adding negated condition...%l (i%u)\n", c, c->inst->i_id);
            /* --- negative or nc cond's are either grounds or potentials --- */
            add_to_chunk_cond_set(&negated_set, make_chunk_cond_for_negated_condition(c));
            if (thisAgent->sysparams[TRACE_BACKTRACING_SYSPARAM])
            {
                push(thisAgent, c, negateds_to_print);
            }
        }
    }

    /* --- scan through conditions, collect grounds, potentials, & locals --- */
    grounds_to_print = NIL;
    locals_to_print = NIL;
    negateds_to_print = NIL;

    /* --- if tracing BT, print the resulting conditions, etc. --- */
    if (thisAgent->sysparams[TRACE_BACKTRACING_SYSPARAM])
    {
        /* mvp 5-17-94 */
        thisAgent->outputManager->printa(thisAgent, "  -->Grounds:\n");
        xml_begin_tag(thisAgent, kTagGrounds);
        print_consed_list_of_condition_wmes(thisAgent, grounds_to_print, 0);
        xml_end_tag(thisAgent, kTagGrounds);
        thisAgent->outputManager->printa(thisAgent,  "\n");
        thisAgent->outputManager->printa(thisAgent, "  -->Locals:\n");
        xml_begin_tag(thisAgent, kTagLocals);
        print_consed_list_of_condition_wmes(thisAgent, locals_to_print, 0);
        xml_end_tag(thisAgent, kTagLocals);
        thisAgent->outputManager->printa_sf(thisAgent,  "\n");
        thisAgent->outputManager->printa(thisAgent, "  -->Negated:\n");
        xml_begin_tag(thisAgent, kTagNegated);
        print_consed_list_of_conditions(thisAgent, negateds_to_print, 0);
        xml_end_tag(thisAgent, kTagNegated);
        thisAgent->outputManager->printa_sf(thisAgent,  "\n");
        /* mvp done */

        xml_begin_tag(thisAgent, kTagNots);
        xml_begin_tag(thisAgent, kTagNot);
        xml_end_tag(thisAgent, kTagNot);
        xml_end_tag(thisAgent, kTagNots);
        xml_end_tag(thisAgent, kTagBacktrace);
    }

    /* Moved these free's down to here, to ensure they are cleared even if we're
       not printing these lists     */

    free_list(thisAgent, grounds_to_print);
    free_list(thisAgent, locals_to_print);
    free_list(thisAgent, negateds_to_print);
}

/* ---------------------------------------------------------------
                             Trace Locals

   This routine backtraces through locals, and keeps doing so until
   there are no more locals to BT.
--------------------------------------------------------------- */

void Explanation_Based_Chunker::trace_locals(goal_stack_level grounds_level)
{

    /* mvp 5-17-94 */
    cons* c, *CDPS;
    condition* cond;
    preference* bt_pref, *p;

    dprint(DT_BACKTRACE, "Tracing locals...\n");
    if (thisAgent->sysparams[TRACE_BACKTRACING_SYSPARAM])
    {
        thisAgent->outputManager->printa(thisAgent, "\n\n*** Tracing Locals ***\n");
        xml_begin_tag(thisAgent, kTagLocals);
    }

    while (locals)
    {
        c = locals;
        locals = locals->rest;
        cond = static_cast<condition_struct*>(c->first);
        free_cons(thisAgent, c);

        if (thisAgent->sysparams[TRACE_BACKTRACING_SYSPARAM])
        {
            thisAgent->outputManager->printa(thisAgent, "\nFor local ");
            xml_begin_tag(thisAgent, kTagLocal);
            print_wme(thisAgent, cond->bt.wme_);
            thisAgent->outputManager->printa(thisAgent, " ");
        }
        thisAgent->outputManager->set_print_test_format(true, true);
        dprint(DT_BACKTRACE, "Backtracing through local condition %l...\n", cond);
        thisAgent->outputManager->clear_print_test_format();
        bt_pref = find_clone_for_level(cond->bt.trace, static_cast<goal_stack_level>(grounds_level + 1));

        if (bt_pref)
        {
            backtrace_through_instantiation(bt_pref->inst, grounds_level, cond, bt_pref->o_ids, bt_pref->rhs_funcs, cond->inst->explain_depth, BT_Normal);

            if (cond->bt.CDPS)
            {
                for (CDPS = cond->bt.CDPS; CDPS != NIL; CDPS = CDPS->rest)
                {
                    p = static_cast<preference_struct*>(CDPS->first);
                    if (thisAgent->sysparams[TRACE_BACKTRACING_SYSPARAM])
                    {
                        thisAgent->outputManager->printa(thisAgent, "     Backtracing through CDPS preference: ");
                        xml_begin_tag(thisAgent, kTagCDPSPreference);
                        print_preference(thisAgent, p);
                    }

                    backtrace_through_instantiation(p->inst, grounds_level, NULL, p->o_ids, p->rhs_funcs, cond->inst->explain_depth, BT_CDPS);

                    if (thisAgent->sysparams[TRACE_BACKTRACING_SYSPARAM])
                    {
                        xml_end_tag(thisAgent, kTagCDPSPreference);
                    }
                }
            }

            if (thisAgent->sysparams[TRACE_BACKTRACING_SYSPARAM])
            {
                xml_end_tag(thisAgent, kTagLocal);
            }
            continue;
        }

        if (thisAgent->sysparams[TRACE_BACKTRACING_SYSPARAM])
        {
            thisAgent->outputManager->printa(thisAgent, "...no trace, can't BT");
            // add an empty <backtrace> tag to make parsing XML easier
            xml_begin_tag(thisAgent, kTagBacktrace);
            xml_end_tag(thisAgent, kTagBacktrace);
        }
        /* --- for augmentations of the local goal id, either handle the "^quiescence t" test or discard it --- */
        Symbol* thisID = cond->data.tests.id_test->eq_test->data.referent;
        Symbol* thisAttr = cond->data.tests.attr_test->eq_test->data.referent;
        Symbol* thisValue = cond->data.tests.value_test->eq_test->data.referent;
        if (thisID->id->isa_goal)
        {
            if ((thisAttr == thisAgent->symbolManager->soarSymbols.quiescence_symbol) &&
                (thisValue == thisAgent->symbolManager->soarSymbols.t_symbol) &&
                (! cond->test_for_acceptable_preference))
            {
                m_reliable = false;
            }
            if (thisAgent->sysparams[TRACE_BACKTRACING_SYSPARAM])
            {
                xml_end_tag(thisAgent, kTagLocal);
            }
            continue;
        }

        dprint(DT_BACKTRACE, "--! Local condition removed (no trace): %l.\n", cond);

        if (thisAgent->sysparams[TRACE_BACKTRACING_SYSPARAM])
        {
            xml_end_tag(thisAgent, kTagLocal);
        }

    } /* end of while locals loop */

    if (thisAgent->sysparams[TRACE_BACKTRACING_SYSPARAM])
    {
        xml_end_tag(thisAgent, kTagLocals);
    }
}

/* ---------------------------------------------------------------
                       Trace Grounded Potentials

   This routine looks for positive potentials that are in the TC
   of the ground set, and moves them over to the ground set.  This
   process is repeated until no more positive potentials are in
   the TC of the grounds.
--------------------------------------------------------------- */

/* Requires: pCond is a local condition */
void Explanation_Based_Chunker::add_local_singleton_unification_if_needed(condition* pCond)
{
    if (pCond->bt.wme_->id->id->isa_goal)
    {
        if (pCond->bt.wme_->attr == thisAgent->symbolManager->soarSymbols.superstate_symbol)
        {
            if (!local_singleton_superstate_identity)
            {
                dprint(DT_UNIFY_SINGLETONS, "Storing identities for local singleton wme: %l\n", pCond);
                local_singleton_superstate_identity = new identity_triple(pCond->data.tests.id_test->eq_test->identity,
                    pCond->data.tests.attr_test->eq_test->identity, pCond->data.tests.value_test->eq_test->identity);
            } else {
                dprint(DT_UNIFY_SINGLETONS, "Unifying local singleton wme: %l\n", pCond);
                if (pCond->data.tests.id_test->eq_test->identity || local_singleton_superstate_identity->id)
                {
                    dprint(DT_UNIFY_SINGLETONS, "Unifying identity element %u -> %u\n", pCond->data.tests.id_test->eq_test->identity, local_singleton_superstate_identity->id);
                    add_identity_unification(pCond->data.tests.id_test->eq_test->identity, local_singleton_superstate_identity->id);
                }
                if (pCond->data.tests.attr_test->eq_test->identity || local_singleton_superstate_identity->attr)
                {
                    dprint(DT_UNIFY_SINGLETONS, "Unifying attr element %u -> %u\n", pCond->data.tests.attr_test->eq_test->identity, local_singleton_superstate_identity->attr);
                    add_identity_unification(pCond->data.tests.attr_test->eq_test->identity, local_singleton_superstate_identity->attr);
                }
                if (pCond->data.tests.value_test->eq_test->identity || local_singleton_superstate_identity->value)
                {
                    dprint(DT_UNIFY_SINGLETONS, "Unifying value element %u -> %u\n", pCond->data.tests.value_test->eq_test->identity, local_singleton_superstate_identity->value);
                    add_identity_unification(pCond->data.tests.value_test->eq_test->identity, local_singleton_superstate_identity->value);
                }
            }
        }
    }
}

/* Requires: pCond is being added to grounds and is the second condition being added to grounds
 *           that matched a given wme, which guarantees chunker_bt_last_ground_cond points to the
 *           first condition that matched. */
void Explanation_Based_Chunker::add_singleton_unification_if_needed(condition* pCond)
{
    /* MToDo:  Do we need to check if not a proposal?  This seems to already not unify proposals. */
    if (pCond->bt.wme_->id->id->isa_goal)
    {
        if ((pCond->bt.wme_->attr == thisAgent->symbolManager->soarSymbols.operator_symbol) ||
            (pCond->bt.wme_->attr == thisAgent->symbolManager->soarSymbols.superstate_symbol))
        {
            condition* last_cond = pCond->bt.wme_->chunker_bt_last_ground_cond;
            assert(last_cond);
            dprint(DT_UNIFY_SINGLETONS, "Unifying singleton wme already marked: %l\n", pCond);
            dprint(DT_UNIFY_SINGLETONS, " Other cond val: %l\n", pCond->bt.wme_->chunker_bt_last_ground_cond);
            if (pCond->data.tests.id_test->eq_test->identity || last_cond->data.tests.id_test->eq_test->identity)
            {
                dprint(DT_UNIFY_SINGLETONS, "Unifying identity element %u -> %u\n", pCond->data.tests.id_test->eq_test->identity, last_cond->data.tests.id_test->eq_test->identity);
                add_identity_unification(pCond->data.tests.id_test->eq_test->identity, last_cond->data.tests.id_test->eq_test->identity);
            }
            if (pCond->data.tests.attr_test->eq_test->identity || last_cond->data.tests.attr_test->eq_test->identity)
            {
                dprint(DT_UNIFY_SINGLETONS, "Unifying attr element %u -> %u\n", pCond->data.tests.attr_test->eq_test->identity, last_cond->data.tests.attr_test->eq_test->identity);
                add_identity_unification(pCond->data.tests.attr_test->eq_test->identity, last_cond->data.tests.attr_test->eq_test->identity);
            }
            if (pCond->data.tests.value_test->eq_test->identity || last_cond->data.tests.value_test->eq_test->identity)
            {
                dprint(DT_UNIFY_SINGLETONS, "Unifying value element %u -> %u\n", pCond->data.tests.value_test->eq_test->identity, last_cond->data.tests.value_test->eq_test->identity);
                add_identity_unification(pCond->data.tests.value_test->eq_test->identity, last_cond->data.tests.value_test->eq_test->identity);
            }
        }
    }
}

void Explanation_Based_Chunker::report_local_negation(condition* c)
{
    if (thisAgent->sysparams[TRACE_CHUNK_NAMES_SYSPARAM])
    {
        // use the same code as the backtracing above
        list* negated_to_print = NIL;
        push(thisAgent, c, negated_to_print);

        thisAgent->outputManager->printa(thisAgent, "\n*** Chunk won't be formed due to local negation in backtrace ***\n");
        xml_begin_tag(thisAgent, kTagLocalNegation);
        print_consed_list_of_conditions(thisAgent, negated_to_print, 2);
        xml_end_tag(thisAgent, kTagLocalNegation);

        free_list(thisAgent, negated_to_print);
    }
}
