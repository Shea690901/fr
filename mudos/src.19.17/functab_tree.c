/*
 * File: functab_tree.c
 *
 * Description: This module contains functions related to managing
 *      a binary search tree for an object's function table.  The purpose
 *      is to improve function search times from O(n) (linear search)
 *      to O(lg n) (binary search).
 *
 * Please direct inquiries regarding this code to apang@mindlink.bc.ca
 *
 * Note(s):
 *      The tree insertion function is loosely based on Paul Vixie's
 *      public domain implementation of an AVL tree package, posted to
 *      comp.sources.unix, circa 1987.  For more information on
 *      AVL trees in general, I refer you to:
 *
 *        Knuth, D. "The Art of Computer Programming". Addison-Wesley.
 *          Volume 3.  1973.
 *        Wirth, N. "Algorithms and Data Structures".  Prentice-Hall.
 *          1986
 *
 *      You must #define OPTIMIZE_FUNCTION_TABLE_SEARCH in options.h
 *      in order to enable this driver feature.
 *
 *      Amylaar did not write this.  :^)
 */

/*
 * minimal includes here, in case this module isn't used
 */
#include "config.h"

#ifdef OPTIMIZE_FUNCTION_TABLE_SEARCH
#include <stdio.h>

#include "lint.h"
#include "exec.h"

/*
 * The function table tree is accomplished by augmenting the function
 * structure with fields for the left, right, and balance fields required
 * for AVL trees.  (See "exec.h".)  The actual function data in a slot
 * is not moved, rather only the branch information is changed.  Thus, the
 * table can still be searched linearly.  Also, F_CALL_FUNCTION_BY_ADDRESS
 * calls don't have to be patched.
 *
 * By using indices instead of using real addresses for the left and right
 * branches, the storage overhead for the tree information is lower.  In
 * addition, the tree information is location independent (ie relocatable).
 * The tradeoff is added CPU time to dereference the function table and
 * compute the actual address.  (This may be rectified in a later version.)
 *
 * The function table slot is preallocated and filled before it gets here.
 * Thus, this code does not call malloc().  This also means we never
 * have to delete the tree or any individual node explicity, as this is
 * free()'d when the program itself is free()'d.
 *
 * The function table tree has to be rebuilt when binaries are loaded
 * since string pointers (as opposed to the contents) have likely changed.
 * On the up side, the function table tree isn't stored in saved binaries,
 * where it can increase your disk usage.
 */

/*
 * Prototypes for local functions
 */
static void functab_tree_add(struct function *, signed int *, int, signed short *);

/*
 * The address comparison routine.
 *
 * Since we're using shared strings for function names, comparing only
 * the address (vs the contents) speeds up the search.
 */
#define compare_addrs(x,y) (x < y ? -1 : (x > y ? 1 : 0))

/*
 * Perform a binary tree search in specified function table
 * for the named function.  Returns index into function table.
 *
 * Example of usage:
 *   fnum = lookup_function(prog->p.i.functions, prog->p.i.tree_r, name);
 */
int lookup_function(functab, index, funcname)
    struct function *functab;   /* function table to search */
    int index;                  /* initially, functab tree's root */
    char *funcname;             /* name of function */
{
    signed int i;

    while (index != -1) {
        i = compare_addrs(funcname, functab[index].name);

        if (i < 0) {
            index = functab[index].tree_l;
            continue;
        }

        if (i > 0) {
            index = functab[index].tree_r;
            continue;
        }

        /*
         * We have a match!
         */
        break;
    }

    return index;
}

/*
 * Entry stub to functab_tree_add() where the real work is done.
 *
 * Example of usage:
 *   add_function(prog->p.i.functions, &prog->p.i.tree_r, fnum);
 */
void add_function(functab, root, newfunc)
    struct function *functab;   /* function table */
    signed int *root;           /* functab tree's root */
    int newfunc;                /* table position of new function */
{
    signed short balance = 0;

    functab[newfunc].tree_l = -1;
    functab[newfunc].tree_r = -1;
    functab[newfunc].tree_b = 0;

    functab_tree_add(functab, root, newfunc, &balance);

    return;
}

/*
 *
 */
static void functab_tree_add(functab, root, newfunc, balance)
    struct function *functab;
    signed int *root;
    int newfunc;
    signed short *balance;
{
    signed int i;
    signed int p1, p2; /* temporary "pointers" */

    /*
     * Are we at an insertion point?
     *   If so, add new node here, rebalance, and exit.
     */
    if (*root == -1) {
        *root = newfunc;
        *balance = 1;
        return;
    }

    /*
     * Compare data
     */
    i = compare_addrs(functab[newfunc].name, functab[*root].name);

    if (i < 0) {
        functab_tree_add(functab, &functab[*root].tree_l, newfunc, balance);
        if (*balance) {
            /*
             * left branch has grown
             */
            switch (functab[*root].tree_b) {
              case 1:
                /*
                 * right branch WAS longer; balance is ok now
                 */
                functab[*root].tree_b = 0;
                *balance = 0;
                break;
              case 0:
                /*
                 * balance WAS okay; now left branch longer
                 */
                functab[*root].tree_b = -1;
                break;
              case -1:
                /*
                 * left branch was already too long; rebalance
                 */
                p1 = functab[*root].tree_l;
                if (functab[p1].tree_b == -1) {
                    /*
                     * LL
                     */
                    functab[*root].tree_l = functab[p1].tree_r;
                    functab[p1].tree_r = *root;
                    functab[*root].tree_b = 0;
                    *root = p1;
                } else {
                    /*
                     * double LR
                     */
                    p2 = functab[p1].tree_r;
                    functab[p1].tree_r = functab[p2].tree_l;
                    functab[p2].tree_l = p1;

                    functab[*root].tree_l = functab[p2].tree_r;
                    functab[p2].tree_r = *root;

                    if (functab[p2].tree_b == -1)
                        functab[*root].tree_b = 1;
                    else
                        functab[*root].tree_b = 0;

                    if (functab[p2].tree_b == 1)
                        functab[p1].tree_b = -1;
                    else
                        functab[p1].tree_b = 0;

                    *root = p2;
                }
                functab[*root].tree_b = 0;
                *balance = 0;
            }
        }
        return;
    }

    if (i > 0) {
        functab_tree_add(functab, &functab[*root].tree_r, newfunc, balance);
        if (*balance) {
            /*
             * right branch has grown
             */
            switch (functab[*root].tree_b) {
              case -1:
                /*
                 * left branch WAS longer; balance is ok now
                 */
                functab[*root].tree_b = 0;
                *balance = 0;
                break;
              case 0:
                /*
                 * balance WAS okay; now right branch longer
                 */
                functab[*root].tree_b = 1;
                break;
              case 1:
                /*
                 * right branch was already too long; rebalance
                 */
                p1 = functab[*root].tree_r;
                if (functab[p1].tree_b == 1) {
                    /*
                     * RR
                     */
                    functab[*root].tree_r = functab[p1].tree_l;
                    functab[p1].tree_l = *root;
                    functab[*root].tree_b = 0;
                    *root = p1;
                } else {
                    /*
                     * double RL
                     */
                    p2 = functab[p1].tree_l;
                    functab[p1].tree_l = functab[p2].tree_r;
                    functab[p2].tree_r = p1;

                    functab[*root].tree_r = functab[p2].tree_l;
                    functab[p2].tree_l = *root;

                    if (functab[p2].tree_b == 1)
                        functab[*root].tree_b = -1;
                    else
                        functab[*root].tree_b = 0;

                    if (functab[p2].tree_b == -1)
                        functab[p1].tree_b = 1;
                    else
                        functab[p1].tree_b = 0;

                    *root = p2;
                }
                functab[*root].tree_b = 0;
                *balance = 0;
            }
        }
        return;
    }

    /*
     * Oh...oh.  This is the same key!  This is not good...
     */
    *balance = 0;

#ifdef DEBUG
    debug_message(stderr, "Duplicate entry in function table tree, [%s].\n",
          functab[newfunc].name);
#endif

    return;
}

#endif /* OPTIMIZE_FUNCTION_TABLE_SEARCH */
