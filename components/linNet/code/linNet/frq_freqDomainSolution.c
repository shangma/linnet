/**
 * @file frq_freqDomainSolution.c
 *   Representation of a computed algebraic solution of the LES in the frequency domain.\n
 * Design considerations:\n
 *   The input to a frequency domain solution is a full algebraic solution. Full means, we
 * have a result expression for each unknown, which are all unknowns of the LES plus
 * user-defined voltages, which are sums of the former. The first thing we do is to take a
 * sub-set of \a m of these unknowns, according to the user-defined result. All further
 * steps are done to the unknowns of this sub-set and all other unknowns are simply
 * ignored.\n
 *   The frequency solution object (frq object) shapes an rectangular array of \a m times
 * \a n normalized numerators plus a normalized common denominator. (\a n is the number of
 * knowns or system inputs). The result expression of each of the \a m unknowns has \a n
 * addends, where each addend is the quotient of the according numerator and the common
 * denominator times the according known.\n
 *   Each of the \a m * \a n +1 expressions is a frequency domain expression; it is a sum
 * of addends, where each addend is the product of a rational number, the device constants
 * (mainly R, L, C) to their individual powers and s to its individual power. The addends
 * have been derived by a straightforward transformation from the addends of the algebraic
 * solution. The latter use set bits for the presence of a device in a product and the
 * rational coefficient of the addend is just a sign, i.e. either one or minus one. A present
 * device always means the complex conductivity of that device. In the frq object, this
 * conductivity is explicitly expressed, by substituting an R by 1/R, a C by C*s or an L by
 * 1/(L*s). Furthermore, if a device constant is specified as a relation to another one,
 * like R2=2*R1, than this relation is inserted into the transformed expression. This is,
 * where individual powers of device constants other than 0, +1 or -1 and numeric
 * coefficients other than +1 or -1 result from. This transformation ends up with the
 * previously described form of addends, where all individual powers are signed.\n
 *   A normalized expression is derived from the result expression of the transformation
 * described in the previous paragraph by determining a factor such that the remaining
 * expression has a minimum individual power of null for each device constant and for the
 * frequency variable s. Where "minimum" is meant across all addends of the expression.
 * There will be at least one addend having s^0, one (other) addend having R1^0, one
 * (other) addend having R2^0, and on so on. Negative powers do no longer occur. Moreover,
 * "normalized" means, that the numeric coefficient of each addend, which is an arbitrary
 * rational number prior to normalization, becomes a signed integer number, i.e. the
 * denominator of this coefficient is one. The product of the determined factor and the
 * remaining expression is identical to the result expression of the transformation
 * described in the previous paragraph.\n
 *   The rest of the computation is done as part of the result representation, in the output
 * functions either as human readable text or as Octave script code.\n
 *   One result expression is written per unknown. It is basically a list of quotients of
 * two of the normalized expressions. The list has as many elements as there are system
 * inputs. (In the output the list elements are concatenated by purely static, trivial
 * context. The Octave code requires vectors of the list elements by separating semicolons
 * and embracing rectangular brackets and the human readable text will insert the operator
 * + and the product with the named system input.)\n
 *   The first pass of the result output iterates over all quotients. Comparing the
 * normalization factors of numerator and denominator it's straightforward to find the
 * common terms for cancellation of the ratio. The remaining expressions are only checked
 * for identity; they can be canceled in their entirety if so. (There's no chance to find
 * more complex common divisors.) After cancellation the two expressions are denormalized
 * by multiplying the canceled normalization factors into the remaining expressions. It's
 * guaranteed that both expressions still have individual powers greater or equal to null
 * and numeric coefficients with denominator one. (Cancellation has been done
 * accordingly.)\n
 *   To find common terms, the two expressions are put into a map of such and this addend
 * of the result is now represented as a pair of references to the two expressions in the
 * map.\n
 *   Inserting into the map means the expressions are compared to each of the already
 * contained expressions. If found the reference to the existing expression is returned, if
 * not the expression is added and the reference to the new map entry is returned.
 * Furthermore, the map entry has a Boolean indication, if a stored expression is used by
 * at least one denominator. When entering the denominator expression, this flag is set to
 * true for the according stored expression.\n
 *   All expressions are printed as assignments in the form: symbol name, operator =, right
 * hand side (RHS). Reusing expressions means that RHS is either printed as an expression
 * in all details or by referencing the name of the first assignment, where this expression
 * was already used. It's convenient for human readable output and essential for Octave
 * script output that references appear only by back references (references to already
 * printed assignments). Furthermore, it's convenient to preferably have references to
 * already used denominators rather than to already used numerators. To meet these demands
 * we do the actual output in two addition passes.\n
 *   In the next pass we iterate over all unknowns and all of their terms. (Such a term is
 * a quotient, represented by a pair of references into the expression map.) Each element
 * in the map has a field name, which is empty at the beginning of this pass and which will
 * hold the symbol name used for this expression. If - for the unknown under progress - all
 * referenced numerators either have a non empty name field or they are marked not to be
 * referenced by at least one denominator, then this result is written. The output uses
 * expression writing in all details as RHS if the name field is still empty or the name as
 * RHS otherwise. If the name field had been empty then the name of the numerator or
 * denominator the expression is assigned to is entered as name of the expression in this
 * field. The unknown is marked as already handled.\n
 *  In the next pass we iterate over all not yet handled unknowns. The result is now
 * written unconditionally. If a name field of a denominator is still empty, then the name
 * of this denominator is inserted (and the insertion is kept in mind). For each term
 * (quotient) numerator and denominator expressions are written. If a numerator name field
 * is still empty then the name of this numerator is entered and the RHS is written in all
 * details. Otherwise the RHS is just the already contained name. If a denominators name
 * had been entered in this pass than the RHS is written in all details otherwise just as
 * the already contained name.\n
 *   The two passes of output are slightly modified to optimize a very frequent situation.
 * Often, all terms of all the unknowns have the exactly same denominator (as individual
 * canceling is impossible). In this case, it's more convenient to write this denominator
 * only once at the beginning (in all details). This is done if there's only one expression
 * marked as referenced by a denominator and at least two result terms (i.e. the number of
 * unknowns or the number of system inputs is greater than one).
 *
 * Copyright (C) 2013-2014 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */
/* Module interface
 *   frq_initModule
 *   frq_shutdownModule
 *   frq_createFreqDomainSolution
 *   frq_cloneByReference
 *   frq_cloneByConstReference
 *   frq_deleteFreqDomainSolution
 *   frq_getNameOfIndependent
 *   frq_getNameOfDependent
 *   frq_logFreqDomainSolution
 *   frq_exportAsMCode
 * Local functions
 *   newExpressionAddend
 *   expressionAddendNull
 *   expressionAddendOne
 *   isExpressionAddendNull
 *   freeExpressionAddend
 *   freeExpression
 *   normalizedExpressionNull
 *   normalizedExpressionOne
 *   isNormalizedExpressionNull
 *   freeNormalizedExpression
 *   freeConstString
 *   cmpExprAddendPower
 *   getNoAddendsOfSamePowerOfS
 *   estimateSignOfExpression
 *   isEqualExpressions
 *   isAbsEqualExpressions
 *   mulByAddend
 *   mulByAddendAndCpy
 *   divByAddend
 *   addAddendToExpr
 *   initializeNameArys
 *   transformAddend
 *   getNormalizationFactor
 *   createNormalizedExpression
 *   transformExpression
 *   getBlankTabString
 *   print
 *   printExpression
 *   printCoefInSAsMCode
 *   isExpressionSimple
 *   printNamedExpression
 *   printNamedExpressionAsMCode
 *   cancelFraction
 *   moveExprIntoMap
 *   createExpressionMap
 *   deleteExpressionMap
 *   setExpressionNames
 *   determineOrderOfRendering
 *   setNameOfExpression
 *   getExpression
 *   printSolution
 */

/*
 * Include files
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include "smalloc.h"
#include "snprintf.h"
#include "log_logger.h"
#include "coe_coefficient.inlineInterface.h"
#include "sol_solver.h"
#include "crm_createMatrix.h"
#include "frq_freqDomainSolution.h"
#include "msc_mScript.h"
#include "frq_freqDomainSolution.inlineInterface.h"


/*
 * Defines
 */


/*
 * Local type definitions
 */

/** The origin of a result expression means the address of the term of a frequency domain
    solution, which was name giving to the result expression. If several such terms are
    identical, then only one of them will be the name giving one. This term is addressed to
    by index of dependent, index of independent and the distinction between numerator and
    denominator. */
typedef struct resultExpressionOrigin_t
{
    /** The name giving term belongs to the dependent in the frequency domain solution
        having this index. */
    unsigned int idxDependent;

    /** The name giving term belongs to the independent in the frequency domain solution
        having this index. */
    unsigned int idxIndependent;

    /** Is the name giving term a numerator or a denominator term? */
    boolean isNumerator;

} resultExpressionOrigin_t;


/** The result representation uses a map of (reusable) expressions, the solution is
    composed from. */
typedef struct resultExpression_t
{
    /** The expression has a name. This field is set to NULL until the name is defined. */
    const char *name;

    /** The expression as an ordinary, not normalized list of addends. */
    const frq_frqDomExpression_t *pExpr;

    /** Boolean flag, if the expression is used at least once as denominator. */
    boolean isUsedAsDenom;

    /** The identification of the sub-term of a frequency domain solution, which was name
        giving to this expression. This field is invalid as long as the name is still
        undefined, i.e. as long as \a name is NULL. */
    resultExpressionOrigin_t origin;

} resultExpression_t;


/** Prior to a textual representation, the solution is brought into this structure. It
    supports cancelled fractions and reuse of same expressions. */
typedef struct resultExpressionMap_t
{
    /** A reference to the solution object this map had been created from/for. */
    const frq_freqDomainSolution_t *pSolution;

    /** The number of stored, named expressions. */
    unsigned int noResExpr;

#ifdef DEBUG
    /** The size of the map. */
    unsigned int maxNoResExpr;
#endif

    /** The map of (occasionally) reusable expressions is a simple linear array of those. */
    resultExpression_t *resExprAry;

    /** The \a noDependents times \a noIndependents terms of a solution have each a
        numerator and a denominator. Each of these is represented here as an index into \a
        pExprAry. Here the array for the numerators. */
    unsigned int **idxNumExprAry;

    /** The \a noDependents times \a noIndependents terms of a solution have each a
        numerator and a denominator. Each of these is represented here as an index into \a
        pExprAry. Here the array for the denominators. */
    unsigned int **idxDenomExprAry;

    /** The index into the linear array of stored, reusable expressions can be tagged with
        a set MSB. This bit indicates that the stored expression is the negated, actual
        expression. */
#define RESULT_EXPR_REF_IS_NEGATED  ((UINT_MAX)-(INT_MAX))

} resultExpressionMap_t;


/*
 * Local prototypes
 */

/** Prototype of a pair of functions to create and delete a matrix of array indexes. */
CRM_DECLARE_CREATE_MATRIX( /* context */       unsignedInt
                         , /* elementType_t */ unsigned int
                         )

/** Prototype of a pair of functions to create and delete a matrix of frequency domain
    expressions. */
CRM_DECLARE_CREATE_MATRIX(/* context */        normalizedFrqDomExpression
                         , /* elementType_t */ frq_normalizedFrqDomExpression_t *
                         )

/** Prototype of a pair of functions to create and delete a matrix of string constants. */
CRM_DECLARE_CREATE_MATRIX( /* context */       string
                         , /* elementType_t */ const char *
                         )


/*
 * Data definitions
 */

/** A global logger object is referenced from anywhere for writing progress messages. */
static log_hLogger_t _log = LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT;


/** The module wide accessible heap for addends of a frequency domain expression. */
static mem_hHeap_t _hHeapOfAddends = MEM_HANDLE_INVALID_HEAP;


#ifdef DEBUG
/** A global counter of all references to any created solution object. Used to detect
    memory leaks. */
static unsigned int _noRefsToSolutionObjects = 0;

/** A global counter of all references to any created normalized expression object. Used to
    detect memory leaks. */
static unsigned int _noRefsToExprObjects = 0;
#endif


/*
 * Inline functions
 */

/**
 * Return an uninitialized addend object. It has been allocated using the
 * global heap mem_hHeap_t _hHeapOfAddends and needs to be freed after use with void
 * mem_free(heap_t *, void *).
 *   @return
 * Get a pointer to the new object.
 */

static inline frq_frqDomExpressionAddend_t *newExpressionAddend()
{
    return (frq_frqDomExpressionAddend_t*)mem_malloc(_hHeapOfAddends);

} /* End of newExpressionAddend */




/**
 * Return a addend object, that represents null. It has been allocated using the
 * global heap mem_hHeap_t _hHeapOfAddends and needs to be freed after use with void
 * mem_free(heap_t *, void *).
 *   @return
 * Get a pointer to the new object.
 */

static inline frq_frqDomExpressionAddend_t *expressionAddendNull()
{
    return NULL;

} /* End of expressionAddendNull */




/**
 * Return an addent of a addend object, that represents 1. It has been allocated using
 * the global heap mem_hHeap_t _hHeapOfAddends and needs to be freed after use with
 * void mem_free(heap_t *, void *).
 *   @return
 * Get a pointer to the new object.
 */

static inline frq_frqDomExpressionAddend_t *expressionAddendOne()
{
    frq_frqDomExpressionAddend_t *pOne = newExpressionAddend();
    pOne->pNext = NULL;
    pOne->factor = RAT_ONE;
    memset(pOne->powerOfConstAry, /* value */ 0, /* num */ sizeof(pOne->powerOfConstAry));
    pOne->powerOfS = 0;
    return pOne;

} /* End of expressionAddendOne */



/**
 * Check the addend of an expression if it represents null.
 *   @return
 * Get the Boolean answer.
 */

static inline boolean isExpressionAddendNull
                                    (const frq_frqDomExpressionAddend_t * const pAddend)
{
    return pAddend == NULL;

} /* End of isExpressionAddendNull */



/**
 * Free a single addent of a addend object which had been allocated with
 * frq_frqDomExpressionAddend_t *newExpressionAddend(). If the addend is part of a linked
 * list of addends, then the followers of the deleted addend are ignored.
 *   @param pExpressionAddend
 * The pointer to the freed addend object.
 */

static inline void freeExpressionAddend
                                (const frq_frqDomExpressionAddend_t * const pExpressionAddend)
{
    if(!isExpressionAddendNull(pExpressionAddend))
        mem_free(_hHeapOfAddends, (void*)pExpressionAddend);

} /* End of freeExpressionAddend */



/*
 * Free a complete frequency domain expression, which is a linked list of addends that had
 * been allocated with frq_frqDomExpressionAddend_t *newExpressionAddend().
 *   @param pExpression
 * The pointer to the freed expression or the pointer to the head of the linked list of
 * addends of this.
 */

static inline void freeExpression(const frq_frqDomExpression_t * const pExpression)
{
    if(!isExpressionAddendNull(pExpression))
        mem_freeList(_hHeapOfAddends, (void*)pExpression);

} /* End of freeExpression */



/**
 * Return an expression, that represents null. Its addends have been allocated using the
 * global heap mem_hHeap_t _hHeapOfAddends and need to be freed after use with void
 * mem_free(heap_t *, void *).
 *   @return
 * Get a pointer to the new object. After use, it has to be deleted with \a freeNormalizedExpression.
 */

static inline frq_normalizedFrqDomExpression_t *normalizedExpressionNull()
{
    return NULL;

} /* End of normalizedExpressionNull */




#if 0 // Currently not used
/**
 * Return an expression, that represents one. Its addends have been allocated using the
 * global heap mem_hHeap_t _hHeapOfAddends and need to be freed after use with void
 * mem_free(heap_t *, void *).
 *   @return
 * Get a pointer to the new object. After use, it has to be deleted with \a freeNormalizedExpression.
 */

static inline frq_normalizedFrqDomExpression_t *normalizedExpressionOne()
{
    frq_normalizedFrqDomExpression_t *pNewObj =
                                        smalloc( sizeof(frq_normalizedFrqDomExpression_t)
                                               , __FILE__
                                               , __LINE__
                                               );
    pNewObj->pFactor = expressionAddendOne();
    pNewObj->pExpr   = expressionAddendOne();

#ifdef DEBUG
    ++ _noRefsToExprObjects;
#endif
    return pNewObj;

} /* End of normalizedExpressionOne */
#endif



/**
 * Check a normalized expression object if it represents 0.
 *   @return
 * Get the Boolean answer.
 */

static inline boolean isNormalizedExpressionNull
                                (const frq_normalizedFrqDomExpression_t * const pExpr)
{
    return pExpr == NULL;

} /* End of isNormalizedExpressionNull */



/**
 * Free a complete, normalized frequency domain expression, which has a factor addend and
 * the remaining expression, which is a linked list of addends that had all been allocated
 * with frq_frqDomExpressionAddend_t *newExpressionAddend().
 *   @param pExpression
 * The pointer to the freed normalized expression.
 */

static inline void freeNormalizedExpression
                            (const frq_normalizedFrqDomExpression_t * const pExpression)
{
    if(!isNormalizedExpressionNull(pExpression))
    {
        assert(!isExpressionAddendNull(pExpression->pFactor)
               &&  !isExpressionAddendNull(pExpression->pExpr)
              );
        mem_free(_hHeapOfAddends, (void*)pExpression->pFactor);
        mem_freeList(_hHeapOfAddends, (void*)pExpression->pExpr);

        free((frq_normalizedFrqDomExpression_t*)pExpression);

#ifdef DEBUG
        -- _noRefsToExprObjects;
#endif
    }
} /* End of freeNormalizedExpression */




/**
 * Free a string as generated by e.g. stralloccpy. Actually a free of the stdlib but the
 * function argument permits to pass a const char* without a compiler warning complaining
 * about discarding type qualifiers.
 *   @param string
 * The pointer to the malloc allocated character string to be freed again.
 */

static inline void freeConstString(const char * const string)
{
    free((char*)string);

} /* End of freeConstString */


/*
 * Function implementation
 */


/** Generate code for a pair of functions to create and delete a matrix of array indexes. */
CRM_CREATE_MATRIX( /* context */             unsignedInt
                 , /* elementType_t */       unsigned int
                 , /* initialElementValue */ 0u
                 , /* fctFreeElement */      void
                 )

/** Generate code for a pair of functions to create and delete a matrix of frequency domain
    expressions. */
CRM_CREATE_MATRIX( /* context */             normalizedFrqDomExpression
                 , /* elementType_t */       frq_normalizedFrqDomExpression_t *
                 , /* initialElementValue */ normalizedExpressionNull()
                 , /* fctFreeElement */      freeNormalizedExpression
                 )

/** Generate code for a pair of functions to create and delete a matrix of string constants. */
CRM_CREATE_MATRIX( /* context */             string
                 , /* elementType_t */       const char *
                 , /* initialElementValue */ NULL
                 , /* fctFreeElement */      freeConstString
                 )

/**
 * The terms in an expression are sorted to simplify list operations like inserting terms
 * and to unify the look of the terms in the user output. This method is the basis of
 * ordering the terms. The greatest terms in the sense of this method will come first.\n
 *   The sort order is determined by the powers of the frequency variable and the physical
 * device constants. Higher powers come first. The power of the frequency variable has
 * precedence over the power of the physical constants.
 *   @return
 * The result is greater than null if op1 is "greater" than op2, null if they have the same
 * "size" and less than null if op2 is greater than op1.
 *   @param op1
 * First operand of comparison.
 *   @param op2
 * Second operand of comparison.
 *   @param noConst
 * The total number of device constants, which are in use in the given system.
 *   @param ignoreFreqVar
 * For some aspects of output the power of the frequency variable may be ignored. The
 * function behaves as if they were identical in both operators.
 */

static signed int cmpExprAddendPower( const frq_frqDomExpressionAddend_t * const pOp1
                                    , const frq_frqDomExpressionAddend_t * const pOp2
                                    , const unsigned int noConst
                                    , boolean ignoreFreqVar
                                    )
{
    /* Null is less tha any other addend. */
    if(isExpressionAddendNull(pOp1))
        return isExpressionAddendNull(pOp2)? 0: -1;
    else if(isExpressionAddendNull(pOp2))
        return 1;
    else
    {
        signed int res;
        if(!ignoreFreqVar)
            res = pOp1->powerOfS - pOp2->powerOfS;
        else
            res = 0;

        signed int idxConst = (signed int)noConst;
        while(res == 0  &&  --idxConst >= 0)
            res = pOp1->powerOfConstAry[idxConst] - pOp2->powerOfConstAry[idxConst];

        return res;
    }
} /* End of cmpExprAddendPower */




/**
 * Look ahead while printing an expression: Peek into the list of addends how many addends
 * form a sub-sequence of terms with identical power of the frequency variable. This
 * information is used for grouping addends.
 *   @return
 * Get the number of addends, which is greater or equal to 1.
 *   @param pAddend
 * This addend is the reference and count one. Its successor are looked for the same power
 * in s.
 */

static unsigned int getNoAddendsOfSamePowerOfS(const frq_frqDomExpressionAddend_t *pAddend)
{
    /* The first element needs to be defined otherwise there's no reference power. */
    assert(!isExpressionAddendNull(pAddend));
    const signed int powerOfS = pAddend->powerOfS;
    unsigned int noAddends = 0;

    /* The operation is trivial as the addends are sorted in falling power of s. Just
       iterate along the list until the power changes. */
    do
    {
        ++ noAddends;
        pAddend = pAddend->pNext;
    }
    while(!isExpressionAddendNull(pAddend) &&  pAddend->powerOfS == powerOfS);

    return noAddends;

} /* End of getNoAddendsOfSamePowerOfS */




#if 0
/**
 * The sign of a (numerator) expression is figured out. This is called "estimate" as it is
 * not a well-defined operation but rather a common sense decision. A typical result of the
 * circuit computation is
 * infinite, e.g. for the input impedance of an op-amp. In this case the denominator is
 * null. We'd however like to decide between +inf and -inf and this depends on the sign of
 * the numerator. However, the numerator is a complex expression in s and doesn't have a
 * sign just like that. Most intuitive seems to be taking the sign at s=0, i.e. the
 * behavior of the circuit for direct current. However, even this is difficult: If there
 * are several terms with s to the power null than we'd need the specific device values to
 * find the solution - which we don't have. And if there's no such term (expression is null
 * for s=0) then we can't take this decision at all. The function returns a positive sign
 * for unclear situations and -1 only if there's a single term in s^0 and if this term has
 * a negative coefficient.
 *   @return
 * 1 or -1, see description.
 *   @param pNormalizedExpr
 * The normalized expression to test.
 */

static signed int estimateSignOfExpression
                                (const frq_normalizedFrqDomExpression_t *pNormalizedExpr)
{
    /* Due to the sort order of the terms we will find the term with s^0 at the end of the
       list. If we reach the first such term we decide if it is the very last term -
       otherwise there are at least two such terms and a safe decision is impossible. */
    const frq_frqDomExpression_t *pAddend = pNormalizedExpr->pExpr;
    assert(pAddend != NULL);
    do
    {
        if(pAddend->powerOfS > 0)
            pAddend = pAddend->pNext;
        else
            break;
    }
    while(pAddend != NULL);

    signed int signRemainingExpr;
    if(pAddend != NULL  &&  pAddend->pNext == NULL)
    {
        assert(pNormalizedExpr->pFactor != NULL);
        signRemainingExpr = rat_sign(pAddend->factor)
                            * rat_sign(pNormalizedExpr->pFactor->factor);
    }
    else
    {
        /* A positive sign is the default for all undecided situations. */
        signRemainingExpr = 1;
    }

    return signRemainingExpr;

} /* End of estimateSignOfExpression */
#endif



/**
 * Compare two frequency domain expressions on equality. The operand expressions are lists
 * of addends, not normalized expression objects.
 *   @return
 * true if expressions are identical, false otherwise.
 *   @param pOp1
 * The first operand, or the pointer to its heading addend.
 *   @param pOp2
 * The second operand, or the pointer to its heading addend.
 *   @param noConst
 * The total number of device constants, which are in use in the given system.
 */

static boolean isEqualExpressions( const frq_frqDomExpression_t *pOp1
                                 , const frq_frqDomExpression_t *pOp2
                                 , const unsigned int noConst
                                 )
{
    /* Loop over pairs of addends. These can directly be compared to one another as both
       lists are sorted in a unique order. If the first not equal pair of addends is
       reached we know the result to be false. Only if we reach both lists' ends and the
       comparison is still 0 then we can return true. */
    while(!isExpressionAddendNull(pOp1) && !isExpressionAddendNull(pOp2))
    {
        if(!rat_isEqual(pOp1->factor, pOp2->factor)
           ||  cmpExprAddendPower(pOp1, pOp2, noConst, /* ignoreFreqVar */ false) != 0
          )
        {
            return false;
        }

        pOp1 = pOp1->pNext;
        pOp2 = pOp2->pNext;
    }

    return isExpressionAddendNull(pOp1) && isExpressionAddendNull(pOp2);

} /* End of isEqualExpressions */




/**
 * Compare two frequency domain expressions on absolute equality. The operand expressions
 * are lists of addends, not normalized expression objects.
 *   @return
 * true if expressions are absolute identical, false otherwise.
 *   @param pHaveSameSign
 * \a true is placed in * \a pHaveSameSign if the expressions are entirely identical, false
 * otherwise.
 *   @param pOp1
 * The first operand, or the pointer to its heading addend.
 *   @param pOp2
 * The second operand, or the pointer to its heading addend.
 *   @param noConst
 * The total number of device constants, which are in use in the given system.
 */

static boolean isAbsEqualExpressions( boolean * const pHaveSameSign
                                    , const frq_frqDomExpression_t *pOp1
                                    , const frq_frqDomExpression_t *pOp2
                                    , const unsigned int noConst
                                    )
{
    /* Handle trivial situations: One or both operands are null. */
    if(isExpressionAddendNull(pOp1) && isExpressionAddendNull(pOp2))
    {
        *pHaveSameSign = true;
        return true;
    }
    if(isExpressionAddendNull(pOp1) || isExpressionAddendNull(pOp2))
    {
        *pHaveSameSign = false;
        return false;
    }
    else
    {
        /* If the two operands are candidates for being totally identical or only with
           inverted sign can safely be decided at the very first addend. */
//        const signed int deltaSign = rat_sign(pOp1->factor) * rat_sign(pOp2->factor);
        const boolean haveSameSign = rat_sign(pOp1->factor) == rat_sign(pOp2->factor);

        /* Loop over pairs of addends. These can directly be compared to one another as both
           lists are sorted in a unique order. If the first not equal pair of addends is
           reached we know the result to be false. Only if we reach both lists' ends and the
           comparison is still 0 then we can return true. */
        while(!isExpressionAddendNull(pOp1) && !isExpressionAddendNull(pOp2))
        {
            if(haveSameSign)
            {
                if(!rat_isEqual(pOp1->factor, pOp2->factor))
                    return false;
            }
            else
            {
                if(!rat_isEqual(rat_neg(pOp1->factor), pOp2->factor))
                    return false;
            }

            if(cmpExprAddendPower(pOp1, pOp2, noConst, /* ignoreFreqVar */ false) != 0)
                return false;

            pOp1 = pOp1->pNext;
            pOp2 = pOp2->pNext;
        }

        boolean isAbsEqual = isExpressionAddendNull(pOp1) && isExpressionAddendNull(pOp2);
        *pHaveSameSign = isAbsEqual && haveSameSign;
        return isAbsEqual;
    }
} /* End of isAbsEqualExpressions */



#if 0 // Double-check if this should be kept; is currently not used
/**
 * Multiply a frequency domain expression by a constant addend. The operand is a list of
 * expression addends, not a normalized expression object.\n
 *   The operand is manipulated in place.
 *   @param pExpression
 * The operand expression prior to the call and the result after return.
 *   @param pFactor
 * The other operand, the addend to multiply by.
 *   @param noConst
 * The total number of device constants, which are in use in the given system.
 */

static void mulByAddend( frq_frqDomExpression_t * const pExpression
                       , const frq_frqDomExpressionAddend_t * const pFactor
                       , const unsigned int noConst
                       )
{
    /* This operation does not impact the defined order of terms; a simple
       linear list iteration suffices. */
    frq_frqDomExpressionAddend_t *pAddend = pExpression;
    while(!isExpressionAddendNull(pAddend))
    {
        pAddend->factor = rat_mul(pFactor->factor, pAddend->factor);

        /* For physical reasons, the individual powers of the device constants and of s are
           all in the magnitude of the number of devices and this number is limited to a
           few dozens. A range check is not required for handling the powers. */
        pAddend->powerOfS += pFactor->powerOfS;

        unsigned int idxConst;
        for(idxConst=0; idxConst<noConst; ++idxConst)
            pAddend->powerOfConstAry[idxConst] += pFactor->powerOfConstAry[idxConst];

        pAddend = pAddend->pNext;

    } /* End while(All terms of operand expression) */

} /* End of mulByAddend */
#endif



/**
 * Product of expression and a factor, which is a single addend. Similar to \a mulByAddend
 * but the operation creates a new result object.\n
 *   @return
 * Get the product of the expression * \a pExpression and the addend * \a pFactor as a new
 * denormalized expression object. It has to be freed after use.
 *   @param pExpression
 * The operand expression, which is not changed.
 *   @param pFactor
 * The other operand, the addend to multiply by.
 *   @param noConst
 * The total number of device constants, which are in use in the given system.
 */

static frq_frqDomExpression_t *mulByAddendAndCpy
                                ( const frq_frqDomExpression_t * const pExpression
                                , const frq_frqDomExpressionAddend_t * const pFactor
                                , unsigned int noConst
                                )
{
    /* This operation does not impact the defined order of terms; a simple
       linear list iteration suffices. */
    const frq_frqDomExpressionAddend_t *pAddend = pExpression;
    frq_frqDomExpressionAddend_t *pResult
                               , **ppNext = &pResult;
    while(!isExpressionAddendNull(pAddend))
    {
        frq_frqDomExpressionAddend_t * const pAddendCpy = newExpressionAddend();

        pAddendCpy->factor = rat_mul(pFactor->factor, pAddend->factor);

        /* For physical reasons, the individual powers of the device constants and of s are
           all in the magnitude of the number of devices and this number is limited to a
           few dozens. A range check is not required for handling the powers. */
        pAddendCpy->powerOfS = pAddend->powerOfS + pFactor->powerOfS;

        unsigned int idxConst;
        for(idxConst=0; idxConst<noConst; ++idxConst)
        {
            pAddendCpy->powerOfConstAry[idxConst] = pAddend->powerOfConstAry[idxConst]
                                                    + pFactor->powerOfConstAry[idxConst];
        }

        /* pAddendCpy->pNext is not written here but via ppNext in the next loop cycle. */

        /* Goto the next list element. */
        pAddend = pAddend->pNext;

        /* Put the new addend object at the end of the result list. */
        *ppNext = pAddendCpy;
        ppNext = &pAddendCpy->pNext;

    } /* End while(All terms of operand expression) */

    /* The very last time, we do not link a predecessor and hence the last link pointer is
       still undefined. */
    *ppNext = expressionAddendNull();

    return pResult;

} /* End of mulByAddendAndCpy */




/**
 * Divide a frequency domain expression by a constant addend. The first operand is a list of
 * expression addends, not a normalized expression object.\n
 *   The operand is manipulated in place.
 *   @return
 * Get the result as return value, too. The pointer \a pExpression is returned.
 *   @param pExpression
 * The operand expression prior to the call and the result after return.
 *   @param pDivisor
 * The other operand, the addend to divide by. Must not be null.
 *   @param noConst
 * The total number of device constants, which are in use in the given system.
 */

static frq_frqDomExpression_t *divByAddend( frq_frqDomExpression_t * const pExpression
                                          , const frq_frqDomExpressionAddend_t * const pDivisor
                                          , const unsigned int noConst
                                          )
{
    /* Division by numeric factor is actually done as product with reciprocal value. The
       divisor must not be null, this is double checked by assertion. */
    const rat_num_t factor = rat_reciprocal(pDivisor->factor);

    /* This operation does not impact the defined order of terms; a simple
       linear list iteration suffices. */
    frq_frqDomExpressionAddend_t *pAddend = pExpression;
    while(!isExpressionAddendNull(pAddend))
    {
        pAddend->factor = rat_mul(pAddend->factor, factor);

        /* For physical reasons, the individual powers of the device constants and of s are
           all in the magnitude of the number of devices and this number is limited to a
           few dozens. A range check is not required for handling the powers. */
        pAddend->powerOfS -= pDivisor->powerOfS;

        unsigned int idxConst;
        for(idxConst=0; idxConst<noConst; ++idxConst)
            pAddend->powerOfConstAry[idxConst] -= pDivisor->powerOfConstAry[idxConst];

        pAddend = pAddend->pNext;

    } /* End while(All terms of operand expression) */

    return pExpression;

} /* End of divByAddend */




/**
 * Add a single addend to an expression. The expression is changed in place. It is an
 * ordinary, not normalized exression.
 *   @param ppExpression
 * The manipulated expression is passed as a pointer to the pointer of its head addend. May
 * be a null expression.
 *   @param pNewFreqDomainAddend
 * The addend to add is passed by reference. It may be a null addend. The manipulated
 * expression takes ownership of the object, the caller must no longer use or reference it
 * after return. Particularly, he must not free the addend object.
 *   @param noConst
 * The total number of device constants, which are in use in the given system.
 */

static void addAddendToExpr( frq_frqDomExpression_t ** const ppExpression
                           , frq_frqDomExpressionAddend_t * const pNewFreqDomainAddend
                           , const unsigned int noConst
                           )
{
    if(isExpressionAddendNull(pNewFreqDomainAddend))
        return;

    frq_frqDomExpressionAddend_t **ppNext = ppExpression;
    signed int cmpListWithNew = 1;
    while(!isExpressionAddendNull(*ppNext))
    {
        cmpListWithNew = cmpExprAddendPower( *ppNext
                                           , pNewFreqDomainAddend
                                           , noConst
                                           , /* ignoreFreqVar */ false
                                           );

        if(cmpListWithNew > 0)
        {
            /* The addend under progress has powers that make it be in front of the addend
               to insert. Advance the view to the next addend in the list. */
            ppNext = &(*ppNext)->pNext;
        }
        else /* if(cmpListWithNew <= 0) */
        {
            /* The appropriate location for insertion of the new addend is found as we look
               at an addend with powers already lower than for the new addend. */
            break;
        }
    }

    /* Either the next pointer points to the NULL termination of the list or to the addend
       with lower powers, which thus comes behind the new addend. These cases can both be
       handled as a simple list insertion operation.
         Or it points to an addend with same powers. This addend needs to be combined with
       the new one.
         The situations can be distinguished by cmpListWithNew, which still holds the last
       recent, relevant comparison result. */

    if(cmpListWithNew == 0)
    {
        /* There is already an addend with identical powers. Reuse this addend, just
           combine the two factors. */
        (*ppNext)->factor = rat_add((*ppNext)->factor, pNewFreqDomainAddend->factor);

        /* The new addend is not used as an object inserted into the list. Delete this
           object. */
        freeExpressionAddend(pNewFreqDomainAddend);

        /* The sum can disappear, then remove the list element. */
        assert((*ppNext)->factor.d != 0);
        if((*ppNext)->factor.n == 0)
        {
            frq_frqDomExpressionAddend_t * const pObsoleteNullAddend = *ppNext;
            *ppNext = pObsoleteNullAddend->pNext;
            freeExpressionAddend(pObsoleteNullAddend);
        }
    }
    else
    {
        /* cmpListWithNew < 0: The last recently visited and compared addend has powers
           that make it be behind of the addend to insert.
             cmpListWithNew > 0: We reached the end of the list and look at its
           terminating null object.
             Both cases can be handled by inserting the new addend at the found location
           and placing the vistited addend as its successor. */
        pNewFreqDomainAddend->pNext = *ppNext;
        *ppNext = pNewFreqDomainAddend;
    }
} /* End of addAddendToExpr */




/**
 * Part of creation of a \a frq_freqDomainSolution_t object: The names of the expressions
 * that form numerators and denominators of the solution are derived from the names of
 * dependents and independents and are stored for later use.\n
 *   The other functions \a getNameOfNumerator and \a getNameOfDenominator provide direct
 * access to such a preallocated name.
 *   @param pSolution
 * The half-way completed object under creation. Memory allocation for the string pointers
 * had already been done.
 *   @see const char *getNameOfNumerator( const frq_freqDomainSolution_t * const, unsigned,
 * unsigned)
 *   @see const char *getNameOfDenominator( const frq_freqDomainSolution_t * const,
 * unsigned, unsigned)
 */

static void initializeNameArys(frq_freqDomainSolution_t * const pSolution)
{
    const unsigned int noDependents = frq_getNoDependents(pSolution)
                     , noIndependents = frq_getNoIndependents(pSolution);

    unsigned int idxDependent;
    for(idxDependent=0; idxDependent<noDependents; ++idxDependent)
    {
        unsigned int idxIndependent;
        for(idxIndependent=0; idxIndependent<noIndependents; ++idxIndependent)
        {
            const char * const nameDependent = frq_getNameOfDependent( pSolution
                                                                     , idxDependent
                                                                     )
                     , * const nameIndependent = frq_getNameOfIndependent( pSolution
                                                                         , idxIndependent
                                                                         );
            char nameExpr[sizeof("X__") + strlen(nameDependent) + strlen(nameIndependent)];

            /* The name of the numerator expression. */
#ifdef DEBUG
            signed int noChars =
#endif
            snprintf( nameExpr
                    , sizeof(nameExpr)
                    , "N_%s_%s"
                    , nameDependent
                    , nameIndependent
                    );
            assert((unsigned)noChars+1 == sizeof(nameExpr));
            assert(pSolution->numeratorNameAry[idxDependent][idxIndependent] == NULL);
            pSolution->numeratorNameAry[idxDependent][idxIndependent] = stralloccpy(nameExpr);

            /* The name of the denominator expression can be simply derived from the
               numerator. */
            nameExpr[0] = 'D';
            assert(pSolution->denominatorNameAry[idxDependent][idxIndependent] == NULL);
            pSolution->denominatorNameAry[idxDependent][idxIndependent] =
                                                                stralloccpy(nameExpr);

        } /* End for(All of the independents) */
    } /* End for(All of the dependents) */

} /* End of initializeNameArys */




/**
 * Get the name of a numerator expression as used in the terms of a solution.
 *   @return
 * Get the name as a read-only string. The returned character pointer points into the
 * solution object and is valid as long as the solution object itself.
 *   @param pSolution
 * The pointer to the solution object.
 *   @param idxDependent
 * The index of the dependent, which is identical to the index of the solutions held in
 * this object.
 *   @param idxIndependent
 * The index of the independent or known.
 */

static inline const char *getNameOfNumerator( const frq_freqDomainSolution_t * const pSolution
                                            , unsigned int idxDependent
                                            , unsigned int idxIndependent
                                            )
{
    assert(pSolution->numeratorNameAry[idxDependent][idxIndependent] != NULL);
    return pSolution->numeratorNameAry[idxDependent][idxIndependent];

} /* End of getNameOfNumerator. */




/**
 * Get the name of a denominator expression as used in the terms of a solution.
 *   @return
 * Get the name as a read-only string. The returned character pointer points into the
 * solution object and is valid as long as the solution object itself.
 *   @param pSolution
 * The pointer to the solution object.
 *   @param idxDependent
 * The index of the dependent, which is identical to the index of the solutions held in
 * this object.
 *   @param idxIndependent
 * The index of the independent or known.
 */

static inline const char *getNameOfDenominator
                                        ( const frq_freqDomainSolution_t * const pSolution
                                        , unsigned int idxDependent
                                        , unsigned int idxIndependent
                                        )
{
    assert(pSolution->denominatorNameAry[idxDependent][idxIndependent] != NULL);
    return pSolution->denominatorNameAry[idxDependent][idxIndependent];

} /* End of getNameOfDenominator. */




/**
 * Transform a single addend of an algebraic expression into an equivalent addend of a
 * frequency domain expression. The physical value of the devices constants is substituted
 * by the complex conductance value of the given device.
 *   @return
 * \a true if the transformation could be done, \a false otherwise. An error report has been
 * written to the global application log if \a false should be returned.
 *   @param ppFrqDomExpressionAddend
 * The pointer to the transformed expression addend is returned in * \a
 * ppFrqDomExpressionAddend. The addend is newly allocated and needs to be freed after
 * use.\n
 *   A null addend is returned in case of errors.
 *   @param pAlgebraicAddend
 * The transformed algebraic addend.
 *   @param pTableOfVars
 * The table of variables holds all information about the constants and devices they
 * describe.
 */

static boolean transformAddend( frq_frqDomExpressionAddend_t ** const ppFrqDomExpressionAddend
                              , const coe_coefAddend_t * const pAlgebraicAddend
                              , const tbv_tableOfVariables_t * const pTableOfVars
                              )
{
    boolean success = true;

    /* We create a one to benefit from the side effect of setting most members to null. */
    frq_frqDomExpressionAddend_t *pNewAddend = expressionAddendOne();

    /* The addend is a product of a numeric factor and a set of device constants. The
       factor remains unchanged and all devices are transformed in a loop. */
    assert(pAlgebraicAddend->factor == 1  ||  pAlgebraicAddend->factor == -1);
    pNewAddend->factor.n = pAlgebraicAddend->factor;
    pNewAddend->factor.d = 1;

    const coe_productOfConst_t productOfDevConst = pAlgebraicAddend->productOfConst;
    coe_productOfConst_t maskDev = 0x1;
    unsigned int idxBit = 0;

    /* The loop termination depends on the definition of the type coe_productOfConst_t as
       an unsigned integer. The last term ensures that the loop ends if the left most
       device bit is in use. */
    while(success &&  productOfDevConst >= maskDev  &&  maskDev != 0)
    {
        /* Look for the next device in the product. */
        while((productOfDevConst & maskDev) == 0)
        {
            maskDev <<= 1;
            ++ idxBit;
        }

        /* We access the device description of the device under progress. */
        rat_num_t refFactor;
        unsigned int idxBitRefDev;
        const pci_device_t *pDevice;
        if(tbv_getReferencedDeviceByBitIndex( pTableOfVars
                                            , &refFactor
                                            , &pDevice
                                            , &idxBitRefDev
                                            , idxBit
                                            )
          )
        {
            switch(pDevice->type)
            {
                case pci_devType_conductance:
                    pNewAddend->factor = rat_mul(pNewAddend->factor, refFactor);
                    ++ pNewAddend->powerOfConstAry[idxBitRefDev];
                    break;

                case pci_devType_resistor:
                    pNewAddend->factor = rat_div(pNewAddend->factor, refFactor);
                    -- pNewAddend->powerOfConstAry[idxBitRefDev];
                    break;

                case pci_devType_capacitor:
                    pNewAddend->factor = rat_mul(pNewAddend->factor, refFactor);
                    ++ pNewAddend->powerOfConstAry[idxBitRefDev];
                    ++ pNewAddend->powerOfS;
                    break;

                case pci_devType_inductivity:
                    pNewAddend->factor = rat_div(pNewAddend->factor, refFactor);
                    -- pNewAddend->powerOfConstAry[idxBitRefDev];
                    -- pNewAddend->powerOfS;
                    break;

                case pci_devType_srcUByU:
                case pci_devType_srcUByI:
                case pci_devType_srcIByU:
                case pci_devType_srcIByI:
                    pNewAddend->factor = rat_mul(pNewAddend->factor, refFactor);
                    ++ pNewAddend->powerOfConstAry[idxBitRefDev];
                    break;

                case pci_devType_srcI:
                case pci_devType_srcU:
                case pci_devType_opAmp:
                default: assert(false); break;

            } /* End switch(Which kind of device?) */
        }
        else
            success = false;

        /* Test next bit/device. */
        maskDev <<= 1;
        ++ idxBit;

    } /* End while(Test of all possible device bits) */

    if(success)
        *ppFrqDomExpressionAddend = pNewAddend;
    else
    {
        /* Error: Free the half-way completed addend and return a null object. */
        freeExpressionAddend(pNewAddend);
        *ppFrqDomExpressionAddend = expressionAddendNull();
    }

    return success;

} /* End of transformAddend */




/**
 * Find the normalization factor for a denormalized expression. The expression can be
 * represented as the product of this factor and a remaining expression. For the remaining
 * expression holds:\n
 *   The powers of device constants and of s are not negative and there's at least one
 * addend which uses power null.\n
 *   The numeric coefficient of the addend of the highest power in s is positive.\n
 *   The numeric coefficients of all addends are integer numbers.
 *   @param ppNormAddend
 * The pointer to the addend, which represents the normalization factor, is placed in * \a
 * ppNormAddend. The addend object is newly created and needs to be freed after use.
 *   @param pExpression
 * A pointer to the expression or to the pointer to its head addend. It must
 * not be the null expression, as the operation is not defined for such.
 *   @param noConst
 * The total number of device constants, which are in use in the given system.
 */

static void getNormalizationFactor( frq_frqDomExpressionAddend_t ** const ppNormAddend
                                  , const frq_frqDomExpression_t * const pExpression
                                  , const unsigned int noConst
                                  )
{
    assert(!isExpressionAddendNull(pExpression));
    const frq_frqDomExpressionAddend_t *pAddend = pExpression;

    /* Initialize the result. */
    frq_frqDomExpressionAddend_t *pNormAddend = newExpressionAddend();
    pNormAddend->pNext = expressionAddendNull();
    pNormAddend->powerOfS = INT_MAX;
    assert(noConst <= COE_MAX_NO_CONST);
    unsigned int idxConst;
    for(idxConst=0; idxConst<noConst; ++idxConst)
        pNormAddend->powerOfConstAry[idxConst] = INT_MAX;
    rat_signed_int lcmOfD = 1
                 , gcdOfN = pAddend->factor.n;

    do
    {
        if(pAddend->powerOfS < pNormAddend->powerOfS)
            pNormAddend->powerOfS = pAddend->powerOfS;

        unsigned int idxConst;
        for(idxConst=0; idxConst<noConst; ++idxConst)
        {
            if(pAddend->powerOfConstAry[idxConst] < pNormAddend->powerOfConstAry[idxConst])
                pNormAddend->powerOfConstAry[idxConst] = pAddend->powerOfConstAry[idxConst];
        }

        lcmOfD = rat_lcm(lcmOfD, pAddend->factor.d);
        gcdOfN = rat_gcd(gcdOfN, pAddend->factor.n);

        pAddend = pAddend->pNext;
    }
    while(!isExpressionAddendNull(pAddend));

    /* Adjust sign of factor. */
    pNormAddend->factor = (rat_num_t){.n = gcdOfN, .d = lcmOfD};
    if(rat_sign(pNormAddend->factor) != rat_sign(pExpression->factor))
        pNormAddend->factor = rat_mul(pNormAddend->factor, RAT_MINUS_ONE);

    *ppNormAddend = pNormAddend;

} /* End of getNormalizationFactor */




/**
 * Create a new, normalized expression object from an existing ordinary expression.
 *   @return
 * A new expression object in normalized form is returned. After use, it is deleted by \a
 * freeNormalizedExpression.
 *   @param pExpression
 * An existing expression in not normalized form, i.e. as list of addends.\n
 *   The newly created normalized expression object takes the ownership of all elements of
 * the list. After return, these elements must not be touched any more by the caller.
 *   @param noConst
 * The total number of device constants, which are in use in the given system.
 */

static frq_normalizedFrqDomExpression_t *createNormalizedExpression
                                                ( frq_frqDomExpression_t * const pExpression
                                                , const unsigned int noConst
                                                )
{
    frq_normalizedFrqDomExpression_t *pNewObj =
                                            smalloc( sizeof(frq_normalizedFrqDomExpression_t)
                                                   , __FILE__
                                                   , __LINE__
                                                   );
    if(!isExpressionAddendNull(pExpression))
    {
        pNewObj = smalloc(sizeof(frq_normalizedFrqDomExpression_t), __FILE__, __LINE__);

        /* Find the common elements in the addends of the expression. */
        getNormalizationFactor(&pNewObj->pFactor, pExpression, noConst);

        /* Normalization: Divide the expression by the found factor. We use the in-place
           operation as we gained ownership of the passed in denormalized expression. */
        pNewObj->pExpr = divByAddend(pExpression, pNewObj->pFactor, noConst);
        assert(pNewObj->pExpr->pNext != NULL || rat_isEqual(pNewObj->pExpr->factor, RAT_ONE));

#ifdef DEBUG
        ++ _noRefsToExprObjects;
#endif
    }
    else
        pNewObj = normalizedExpressionNull();

    return pNewObj;

} /* End of createNormalizedExpression */




/**
 * Transform an algebraic expression into an expression in the frequency domain. The
 * algebraic expressions are elements got from the symbolic solver of the LES and the
 * frequency domain expressions are the results wanted by the user.
 *   @return
 * \a true if the transformation could be done, \a false otherwise. An error report has been
 * written to the global application log if \a false should be returned.
 *   @param ppFrqDomExpression
 * The pointer to the transformed expression is returned in * \a ppFrqDomExpression. An
 * invalid, half-way completed expression is returned in case of errors.\n
 *   All the expression's addends are newly allocated and need to be freed after use.
 *   @param pAlgebraicExpr
 * The transformed algebraic expression.
 *   @param pTableOfVars
 * The table of variables holds all information about the physical constants used in the
 * expression and the devices they
 * describe.
 */

static boolean transformExpression
                        ( frq_normalizedFrqDomExpression_t * * const ppFrqDomExpression
                        , const coe_coef_t * const pAlgebraicExpr
                        , const tbv_tableOfVariables_t * const pTableOfVars
                        )
{
    const unsigned int noConst = pTableOfVars->noConstants;

    /* The new expression starts as a null addend. */
    frq_frqDomExpression_t *pNewFreqDomainExpr = expressionAddendNull();

    /* The operation basically is a loop over all addends of the algebraic expression,
       which are transformed into the equivalent frequency domain expression. At the point
       of initial transformation (i.e. now and here) the relationship of addends is one by
       one; only later because of variable substitutions we will be able the aggregate the
       addends of the frequency domain expressions to less addends. */
    const coe_coefAddend_t *pAlgebraicAddend = pAlgebraicExpr;
    boolean success = true;
    while(success && !coe_isCoefAddendNull(pAlgebraicAddend))
    {
        frq_frqDomExpressionAddend_t *pNewFreqDomainExprAddend;
        if(transformAddend(&pNewFreqDomainExprAddend, pAlgebraicAddend, pTableOfVars))
            addAddendToExpr(&pNewFreqDomainExpr, pNewFreqDomainExprAddend, noConst);
        else
            success = false;

        pAlgebraicAddend = pAlgebraicAddend->pNext;

    } /* End while(All addends of the expressions) */

    *ppFrqDomExpression = createNormalizedExpression( pNewFreqDomainExpr
                                                    , noConst
                                                    );
    return success;

} /* End of transformExpression */





/**
 * Generate a blank tabulator string of given length. Consider a previously existing
 * tabualtor string in case of recusively deepend indentation.
 *   @param extendedTabString
 * Result of operation: the new combined tabulator string.
 *   @param sizeOfExtendedTabString
 * Size of result char array. No warning if it doesn't fit: The generated tabulator string
 * will silently be truncated.
 *   @param existingTabString
 * A possibly existing tabulator string. It'll prepend the new, blank part of the tabulator
 * string.
 *   @param additionalIndentation
 * Number of blanks in the new, blank part of the generated tabulator string.
 */

static void getBlankTabString( char * const extendedTabString
                             , unsigned int sizeOfExtendedTabString
                             , const char * const existingTabString
                             , unsigned int additionalIndentation
                             )
{
    snprintf( extendedTabString
            , sizeOfExtendedTabString
            , "%s%*s"
            , existingTabString
            , additionalIndentation
            , ""
            );
} /* End of getBlankTabString */




/**
 * Print formatted to a stream. A wrapper around stdio's printf, which suppresses the
 * stream error messages.
 *   @return
 * The number of printed characters as an unsigned number. If the output yields a stream
 * error message the function returns null.
 *   @param stream
 * The output stream to write to.
 *   @param formatString
 * A printf like format string.
 *   @param ...
 * The arguments of the format string.
 */

static unsigned int print(FILE *stream, const char * const formatString, ...)
{
    va_list argptr;

    va_start(argptr, formatString);
    signed int noChars = vfprintf(stream, formatString, argptr);
    va_end(argptr);

    /* Count the printed characters. Make the distinction between true numbers and
       (negative) error information. */
    unsigned int retVal;
    if(noChars >= 0)
        retVal = noChars;
    else
    {
        /* It might seem that now writing an error message into the application log would
           be an option, but it isn't. This function is only used on the streams of the
           application logger. If we get an error, we can't log the problem into the
           identical erroneous stream. The probablity of an error is close to null and the
           problem will be apparent by getting no ouput. No action is taken. */
        retVal = 0;
    }

    return retVal;

} /* End of print */




/**
 * Print a frequency domain expression. The printed output is not terminated by a final
 * newline. The operand is a list of addends, not a normalized expression object.
 *   @param stream
 * The stream (as provided by the stdio lib) to write to.
 *   @param pExpr
 * Pointer to the expression. It's a list of addends.
 *   @param pTableOfVars
 * The data structure holding all information about the symbolic elements of the system's
 * equations.
 *   @param printMargin
 * Line wrapping takes place when the margin is exceeded but the currently printed addend
 * of the expression is still not completed. Checking the margin only after completing an
 * addend means that the printed lines can become significantly longer than the margin
 * says. So take a rather small value for the margin.
 *   @param tabString
 * This string is written at the beginning of any wrapped line. Normally the empty or a
 * blank string, but could also contain some comment characters, etc.
 */

static void printExpression( FILE * const stream
                           , const frq_frqDomExpression_t * const pExpr
                           , const tbv_tableOfVariables_t * const pTableOfVars
                           , const unsigned int printMargin
                           , const char * const tabString
                           )
{
    /* The tabulator position is derived from the length of the tabulator string. */
    const unsigned int tabPos = strlen(tabString);

    /* The line wrapping assumes that all lines should begin at the same column. This
       implies that the current line already is at the given tabulator position. */
    unsigned int col = tabPos;

    if(isExpressionAddendNull(pExpr))
    {
        /* The format of the ouput in case of a null expression needs to be done in sync
           with the general output code in the else clause. We could also decide to have a
           +0. */
        col += print(stream, "0");
    }
    else
    {
        /* To find out if a term has at least one physical constant we compare it to a
           dummy term representing one. */
        const frq_frqDomExpressionAddend_t * const pAddendOne = expressionAddendOne();

        /* Loop over all groups of addends of the expression. A group is a series of
           addends of same power of the frequency variable. */
        const frq_frqDomExpressionAddend_t *pAddend = pExpr;
        boolean isFirstGroup = true;
        unsigned int groupIndention;
        do
        {
            /* Determine the number of addends (including this one), which have the same
               power of s. */
            const unsigned int sizeOfGroup = getNoAddendsOfSamePowerOfS(pAddend);
            assert(sizeOfGroup >= 1);
            const signed int groupPowerOfS = pAddend->powerOfS;

            /* A group is a sum in parenthesis times the power of s, if there are at least
               two addends in the sum. Otherwise no parenthesis. */

            /* The (outside shown) sign of the group is the sign of the first addend
               inside the group. This way we avoid an opening bracket following by a
               minus. */
            assert(pAddend->factor.n != 0);
            assert(pAddend->factor.d == 1);
            signed int signOfGroup = pAddend->factor.n>0? 1: -1;

            /* The first group suppresses the display of a positive sign in front of
               the opening bracket. */
            if(!isFirstGroup || signOfGroup < 0)
            {
                col += print(stream, "%c", signOfGroup<0? '-': '+');
                groupIndention = 1;
            }
            else
                groupIndention = 0;

            if(sizeOfGroup > 1)
            {
                /* The opening bracket. */
                col += print(stream, "(");
            }

            /* Loop over all addends in the group. */
            const unsigned int noConst = pTableOfVars->noConstants;
            boolean isFirstGroupAddend = true
                  , firstFactor = true
                  , wrappedGroup = false;
            unsigned int idxAddendInGroup;
            for(idxAddendInGroup=0; idxAddendInGroup<sizeOfGroup; ++idxAddendInGroup)
            {
                /* Print a single addend from the group. */
                firstFactor = true;

                /* Write the numeric factor. In general force having a sign but omit the
                   sign of the first addend: this sign has already been written as sign of
                   the group. */
                //frq_coefAddendFactor_t i = signOfGroup * pAddend->factor;
                signed long i = signOfGroup * pAddend->factor.n;
                if(!isFirstGroupAddend)
                    col += print(stream, " %c ", i<0? '-': '+');
                /* Write the number without sign but in case of a one only if it would be
                   the only element of the product. ignoreFreqVar: If the term s^n is taken
                   out of the parenthesis, then we need the explicit 1 even for n!=0. */
                if((i != 1 && i != -1)
                   ||  cmpExprAddendPower( pAddend
                                         , pAddendOne
                                         , noConst
                                         , /* ignoreFreqVar */ sizeOfGroup > 1
                                         )
                       == 0
                  )
                {
                    col += print(stream, "%ld", i<0? -i: i);
                    firstFactor = false;
                }

                signed int idxConst;
                for(idxConst=noConst-1; idxConst>=0; --idxConst)
                {
                    if(pAddend->powerOfConstAry[idxConst] != 0)
                    {
                        if(!firstFactor)
                            col += print(stream, "*");
                        firstFactor = false;

                        const pci_device_t *const pDev = tbv_getDeviceByBitIndex( pTableOfVars
                                                                                , idxConst
                                                                                );
                        col += print(stream, "%s", pDev->name);
                        if(pAddend->powerOfConstAry[idxConst] != 1)
                        {
                            /* Due to expression normalization all powers are positive. Yet
                               we don't need an assertion to check: if it would be negative
                               the output is still widely okay, we'd only set the power
                               into brackets then. */
                            col += print( stream
                                        , "^%d"
                                        , pAddend->powerOfConstAry[idxConst]
                                        );
                        }
                    } /* End if(Tested constant is part of this term?) */
                } /* For(All defined physical constants) */

                /* Print next addend in next cycle. */
                pAddend = pAddend->pNext;

                /* Add a line feed and indentation white space if print margin is exceeded. */
                if(idxAddendInGroup+1 < sizeOfGroup)
                {
                    if(col >= printMargin)
                    {
                        print(stream, "\n%s", tabString);

                        /* Actually, we need to indent two additional blanks to compensate
                           for sign and bracket, but one blank will be written as
                           separation character for the next, wrapped addend. */
                        unsigned u;
                        for(u=0; u<groupIndention; ++u)
                            print(stream, " ");
                        col = tabPos+groupIndention;
                        wrappedGroup = true;
                    }
                }

                isFirstGroupAddend = false;

            } /* End for(All addends of a group) */

            if(sizeOfGroup > 1)
            {
                /* The closing bracket is placed at the end of the line or below its
                   opening counterpart. Maybe we need another line break. */
                if(wrappedGroup)
                {
                    print(stream, "\n%s", tabString);
                    unsigned u;
                    for(u=0; u<groupIndention; ++u)
                        print(stream, " ");
                    col = tabPos+groupIndention;
                }

                col += print(stream, ")");
            }

            /* The group's term s^n is printed only if relevant. */
            if(groupPowerOfS != 0)
            {
                if(!firstFactor)
                    col += print(stream, " *");
                firstFactor = false;

                col += print(stream, " s");
                if(groupPowerOfS != 1)
                    col += print(stream, "^%d", groupPowerOfS);

            } /* End if(Frequency variable s is relevant of this group?) */

            /* Add a line feed and indentation white space after each group. */
            if(!isExpressionAddendNull(pAddend))
            {
                print(stream, "\n%s", tabString);
                col = tabPos;
            }

            isFirstGroup = false;
        }
        while(!isExpressionAddendNull(pAddend)); /* End while(All groups of addends) */

        /* Free the no longer used dummy object. */
        freeExpressionAddend(pAddendOne);

    } /* End if(Special case of a null expression?) */

} /* End of printExpression */




/**
 * Write a coefficient of a polynomial in s as Ocatve M script code into a text stream.
 * Such a coefficient is a sub-list of addends of a frequency domain expression, where all
 * addends of the sub-list have the same power of s.
 *   @return
 * The methods iterates along the expression's addend list until it reaches the first
 * addend of another (i.e. lower) power of s or the end of the list. The advanced pointer
 * to the visited list element is returned. This may be the list terminating NULL pointer.
 *   @param stream
 * The stream (as provided by the stdio lib) to write to.
 *   @param pHeadOfGroup
 * Pointer to the first exported addend of the expression. It should be the first addend
 * out of a group of those having the same power of s.
 *   @param pTableOfVars
 * The data structure holding all information about the symbolic elements of the system's
 * equations.
 *   @param printMargin
 * Line wrapping takes place when the margin is exceeded but the currently printed
 * coefficient is still not completed. Checking the margin only after completing an addend
 * means that the printed lines can become significantly longer than the margin says. So
 * take a rather small value for the margin.
 *   @param tabString
 * This string is written at the beginning of any wrapped line. Normally the empty or a
 * blank string, but could also contain some comment characters, etc.
 */

static const frq_frqDomExpressionAddend_t *
                 printCoefInSAsMCode( FILE * const stream
                                    , const frq_frqDomExpressionAddend_t * const pHeadOfGroup
                                    , const tbv_tableOfVariables_t * const pTableOfVars
                                    , const unsigned int printMargin
                                    , const char * const tabString
                                    )
{
    /* The tabulator position is derived from the length of the tabulator string. */
    const unsigned int tabPos = strlen(tabString);

    /* The line wrapping assumes that all lines should begin at the same column. This
       implies that the current line already is at the given tabulator position. */
    unsigned int col = tabPos;

    assert(!isExpressionAddendNull(pHeadOfGroup));
    const frq_frqDomExpressionAddend_t *pAddend = pHeadOfGroup;

    /* Determine the number of addends (including this one), which have the same
       power of s. */
    const unsigned int sizeOfGroup = getNoAddendsOfSamePowerOfS(pAddend);
    assert(sizeOfGroup >= 1);
    const signed int groupPowerOfS = pAddend->powerOfS;

    /* A group is a sum in parenthesis, if there are at least two addends in the
       sum. Otherwise no parenthesis. The parenthesis is required to permit some
       separating blanks in the expression. Without parenthesis Octave would
       see separated column values. */

    /* The (outside shown) sign of the group is the sign of the first addend
       inside the group. This way we avoid a opening bracket following by a
       minus. */
    assert(pAddend->factor.n != 0);
    assert(pAddend->factor.d == 1);
    rat_signed_int signOfGroup = pAddend->factor.n>0? 1: -1;

    /* A positive sign in front of the only addend or the opening bracket is not displayed. */
    unsigned int groupIndention = 0;
    if(signOfGroup < 0)
    {
        col += print(stream, "-");
        if(sizeOfGroup > 1)
            ++ groupIndention;
        // else: The - belongs to the only addend and doesn't move the tabulator.
    }

    char extendedTabString[128];
    if(sizeOfGroup > 1)
    {
        /* The opening bracket. */
        col += print(stream, "(");
        
        /* The just printed parenthesis is not counted for the indentation since the next
           addend on the new line will start with the pattern " + ", which includes the
           addtional heading blank. */
        getBlankTabString( extendedTabString
                         , sizeof(extendedTabString)
                         , /* existingTabString */ tabString
                         , /* additionalIndentation */ groupIndention
                         );
    }
    else
    {
        /* The single addend to print is not enclosed in parenthesis. */
        
        /* The tab string is actually not used in this case; we only have a single addend
           to print and surely no line break. */
        extendedTabString[0] = '\0';
    }

    /* To find out if a term has at least one physical constant we compare it to a
       dummy term representing one. */
    const frq_frqDomExpressionAddend_t * const pAddendOne = expressionAddendOne();

    /* Loop over all addends in the group. */
    const unsigned int noConst = pTableOfVars->noConstants;
    boolean isFirstAddend = true
          , wrappedGroup = false;
    unsigned int idxAddendInGroup;
    for(idxAddendInGroup=0; idxAddendInGroup<sizeOfGroup; ++idxAddendInGroup)
    {
        /* Print a single addend from the group. */
        boolean firstFactor = true;

        /* Write the numeric factor. In general we force having a sign but we omit the sign
           of the first addend: this sign has already been written as sign of the group. */
        rat_signed_int i = signOfGroup * pAddend->factor.n;
        if(!isFirstAddend)
            col += print(stream, " %c ", i<0? '-': '+');
        /* Write the number without sign but in case of a one only if it would be
           the only element of the product. */
        if((i != 1 && i != -1)
           ||  cmpExprAddendPower( pAddend
                                 , pAddendOne
                                 , noConst
                                 , /* ignoreFreqVar */ true
                                 )
               == 0
          )
        {
            col += print(stream, "%ld", (signed long)(i<0? -i: i));
            firstFactor = false;
        }

        signed int idxConst;
        for(idxConst=noConst-1; idxConst>=0; --idxConst)
        {
            if(pAddend->powerOfConstAry[idxConst] != 0)
            {
                if(firstFactor)
                    firstFactor = false;
                else
                    col += print(stream, "*");

                const pci_device_t * const pDev = tbv_getDeviceByBitIndex( pTableOfVars
                                                                         , idxConst
                                                                         );
                col += print(stream, "%s", pDev->name);
                if(pAddend->powerOfConstAry[idxConst] != 1)
                {
                    /* Due to expression normalization all powers are positive. Yet
                       we don't need an assertion to check: if it would be negative
                       the output is still widely okay, we'd only set the power
                       into brackets then. */
                    col += print( stream
                                , "^%d"
                                , pAddend->powerOfConstAry[idxConst]
                                );
                }
            } /* End if(Tested constant is part of this term?) */
        } /* For(All defined physical constants) */

        /* Print next addend in next cycle. */
        pAddend = pAddend->pNext;

        /* Add a line feed and indentation white space if print margin is exceeded. */
        if(idxAddendInGroup+1 < sizeOfGroup)
        {
            if(col >= printMargin)
            {
                print(stream, " ...\n%s", extendedTabString);
                col = tabPos+groupIndention;
                wrappedGroup = true;
            }
        }

        isFirstAddend = false;

    } /* End for(All addends of a group) */

    /* Free the no longer used dummy object. */
    freeExpressionAddend(pAddendOne);

    /* The closing bracket is placed at the end of the line or below its
       opening counterpart. Maybe we need another line break. */
    if(sizeOfGroup > 1)
    {
        if(wrappedGroup)
        {
            print(stream, " ...\n%s", extendedTabString);
            col = tabPos+groupIndention;
        }
        col += print(stream, ")");
    }

    /* The group's term s^n is printed as comment. */
    col += print(stream, "\t %% s^%d", groupPowerOfS);

    return pAddend;

} /* End of printCoefInSAsMCode */




/**
 * Decide if an expression is simple. This function is used for rendering of the results.
 * If an expression has already benn rendered then normally it won't be rendered again.
 * Instead the already printed expression is referenced by name. However, if an expression
 * is simple (in the sense of this function) it would be rendered again rather than
 * referencing an earlier copy. Most popular examples of such simple examples are a null or
 * a one expression.\n
 *   @return
 * Get the boolean statement if the the passed epression is simple.
 *   @param pExpr
 * The investigated expression.
 *   @param noConst
 * The total number of device constants, which are in use in the given system.
 */

static boolean isExpressionSimple( const frq_frqDomExpression_t * const pExpr
                                 , const unsigned int noConst
                                 )
{
    /* Definition: A simple expression is a purely numeric expression. The power of s and
       the powers of all device constants are null in all terms. */

    if(isExpressionAddendNull(pExpr))
        return true;

    /* An expression with several addends can't have only null powers. */
    if(!isExpressionAddendNull(pExpr->pNext) || pExpr->powerOfS != 0)
        return false;

    /* Check all device powers. */
    unsigned int noConstantWithPowerOne = 0;
    unsigned int idxConstant;
    for(idxConstant=0; idxConstant<noConst; ++idxConstant)
    {
        if(pExpr->powerOfConstAry[idxConstant] == 1)
        {
            /* An expression is considered simple as long as there's only a single device
               constant with poser one and factor +1 or -1. */
            ++ noConstantWithPowerOne;
            if(noConstantWithPowerOne > 1
               ||  (!rat_isEqual(pExpr->factor, RAT_ONE)
                    && !rat_isEqual(pExpr->factor, RAT_MINUS_ONE)
                   )
              )
            {
                return false;
            }
        }
        else if(pExpr->powerOfConstAry[idxConstant] != 0)
            return false;
    }

    return true;

} /* End of isExpressionSimple */




/**
 * Write a frequency domain expression as human readable text into a text stream. The
 * expression is represented as a polynomial in s. The operand is a list of addends, not a
 * normalized expression object.
 *   @param stream
 * The stream (as provided by the stdio lib) to write to.
 *   @param name
 * This string holds the name of the expression. The output is an assignment like: \a
 * name = expression;
 *   @param nameRHS
 * The name of a variable, which can be used as expression on the right hand side of the
 * assignment. If NULL is passed or if the expression * \a pExpr is trivial, then the
 * expression * \a pExpr is printed as RHS, otherwise only this name (as a reference to an
 * already defined other expression).
 *   @param pExpr
 * Pointer to the expression. It's a list of addends.
 *   @param invertSign
 * To improve reusability of expressions the referenced RHS may have the inverse sign. This
 * is regardless whether the RHS is defined by \a nameRHS or by \a pExpr. Pass \a true if
 * the RHS should be multiplied by -1 prior to printing.
 *   @param pTableOfVars
 * The data structure holding all information about the symbolic elements of the system's
 * equations.
 *   @param printMargin
 * Line wrapping takes place when the margin is exceeded but the currently printed addend
 * of the expression is still not completed. Checking the margin only after completing an
 * addend means that the printed lines can become significantly longer than the margin
 * says. So take a rather small value for the margin.
 *   @param tabString
 * This string is written at the beginning of any wrapped line. Normally the empty or a
 * blank string, but could also contain some comment characters, etc.
 */

static void printNamedExpression( FILE * const stream
                                , const char * const name
                                , const char * const nameRHS
                                , const frq_frqDomExpression_t * const pExpr
                                , boolean invertSign
                                , const tbv_tableOfVariables_t * const pTableOfVars
                                , const unsigned int printMargin
                                , const char * const tabString
                                )
{
    const unsigned int noConst = pTableOfVars->noConstants;
    print(stream, "%s%s(s) = ", tabString, name);

    if(nameRHS != NULL  &&  !isExpressionSimple(pExpr, noConst))
    {
        /* Simple: The assigned value is a known, named expression. We just assign the variable
           of that name. */
        print(stream, "%s%s(s)", invertSign? "-": "", nameRHS);
    }
    else
    {
        /* Usual complex case: Print the RHS as an expression in all details. It is a sum
           of products of coefficients with s to falling powers. */

        /* If the sign bit is set then we don't have a ready to use reference to
           the expression to print. Unfortunately, as we can't manipulate the
           returned read-only reference, we need to create a temporary
           (sign-inverted) copy of the expression first. */
        const frq_frqDomExpression_t *pPrintedExpr;
        if(invertSign)
        {
            frq_frqDomExpression_t * const minusOne = expressionAddendOne();
            minusOne->factor = RAT_MINUS_ONE;
            pPrintedExpr = mulByAddendAndCpy(pExpr, minusOne, noConst);
            freeExpressionAddend(minusOne);
        }
        else
            pPrintedExpr = pExpr;

        /* The printing sub-routine will use several lines. Each product of coefficient
           with s^n will begin with the string composed here. We want to see proper
           left-aligned text. */
        char extendedTabString[128];
        getBlankTabString( extendedTabString
                         , sizeof(extendedTabString)
                         , /* existingTabString */ tabString
                         , /* additionalIndentation */
                           strlen(name) + sizeof("(s) = ") - 1
                         );
        printExpression(stream, pPrintedExpr, pTableOfVars, printMargin, extendedTabString);

        if(invertSign)
            freeExpression(pPrintedExpr);
    }

    print(stream, "\n");

} /* End of printNamedExpression */




/**
 * Write a frequency domain expression as Ocatve M script code into a text stream. The
 * expression is represented by the vector of coefficients of the polynomial in s, as
 * expected by Octave functions like tf. The operand is a list of addends, not a normalized
 * expression object.
 *   @param stream
 * The stream (as provided by the stdio lib) to write to.
 *   @param name
 * This string is holds the name of the expression. The output is an assignment like: \a
 * name = expression;
 *   @param nameRHS
 * The name of a variable to be used as expression on the right hand side of the assignment.
 * Only if NULL is passed, then the complex expression * \a pExpr is printed as RHS.
 *   @param pExpr
 * Pointer to the expression. It's a list of addends. Ignored if \a nameRHS is not NULL.
 *   @param invertSign
 * To improve reusability of expressions the referenced RHS may have the inverse sign. This
 * is regardless whether the RHS is defined by \a nameRHS or by \a pExpr. Pass \a true if
 * the RHS should be multiplied by -1 prior to printing.
 *   @param pTableOfVars
 * The data structure holding all information about the symbolic elements of the system's
 * equations. Ignored if \a nameRHS is not NULL.
 *   @param printMargin
 * Line wrapping takes place when the margin is exceeded but the currently printed addend
 * of the expression is still not completed. Checking the margin only after completing an
 * addend means that the printed lines can become significantly longer than the margin
 * says. So take a rather small value for the margin. Ignored if \a nameRHS is not NULL.
 */

static void printNamedExpressionAsMCode( FILE * const stream
                                       , const char * const name
                                       , const char * const nameRHS
                                       , const frq_frqDomExpression_t * const pExpr
                                       , boolean invertSign
                                       , const tbv_tableOfVariables_t * const pTableOfVars
                                       , const unsigned int printMargin
                                       )
{
    const unsigned int noConst = pTableOfVars->noConstants;

    print(stream, "%s = ", name);

    if(nameRHS != NULL  &&  !isExpressionSimple(pExpr, noConst))
    {
        /* Simple: The assigned value is a known, named expression. We just assign the
           variable of that name. */
        print(stream, "%s%s", invertSign? "-": "", nameRHS);
    }
    else
    {
        /* Usual complex case: Print the RHS as an expression in all details. In Octave
           this is a row vector of coefficients in falling power of s. */
        print(stream, "%s[ ",  invertSign? "-": "");

        /* The printing sub-routine will use several lines. Each coefficient (or vector
           element) will begin with the string composed here. We want to see
           proper left-aligned text. */
        char tabStringVecElem[128];
        getBlankTabString( tabStringVecElem
                         , sizeof(tabStringVecElem)
                         , /* existingTabString */ ""
                         , /* additionalIndentation */
                           strlen(name) + sizeof(" = ") - 1 + (invertSign? 1: 0)
                         );

        if(isExpressionAddendNull(pExpr))
        {
            /* The format of the ouput in case of a null expression needs to be done in
               sync with the general output code in the else clause. We could also decide
               to have a +0. */
            print(stream, "0\t %% s^0\n%s", tabStringVecElem);
        }
        else
        {
            /* The indentation for line breaks in between the addends of a coefficient
               needs to consider the "[ " or "; " in front of a coefficient. */
            char tabStringAddend[128];
            getBlankTabString( tabStringAddend
                             , sizeof(tabStringAddend)
                             , /* existingTabString */ tabStringVecElem
                             , /* additionalIndentation */ sizeof("[ ") - 1
                             );

            /* Loop over all powers of the frequency variable s. The addends of the
               expression are ordered in groups of same power of s. Highest powers come
               first, which is the order expected by the Octave functions. However, there
               will be powers in s, which do not occur in the expression and thus in no
               group. Here we have to insert null coefficients into the Octave vector. */
            const frq_frqDomExpressionAddend_t *pAddend = pExpr;
            signed int powerOfS;
            for(powerOfS=pAddend->powerOfS; powerOfS>=0; --powerOfS)
            {
                if(!isExpressionAddendNull(pAddend) &&  pAddend->powerOfS == powerOfS)
                {
                    /* Export the non null coefficient of the term of the next present
                       power in s. */
                    pAddend = printCoefInSAsMCode( stream
                                                 , pAddend
                                                 , pTableOfVars
                                                 , printMargin
                                                 , tabStringAddend
                                                 );
                }
                else
                {
                    /* The expression doesn't contain a term in the next power of s, so we need
                       to write a null value. */
                    print(stream, "0\t %% s^%d", powerOfS);

                } /* End if(Power of s contained in exported expression?) */

                /* Add a line feed and indentation white space after each group. */
                print(stream, "\n%s", tabStringVecElem);
                if(powerOfS > 0)
                    print(stream, "; ");

            } /* End for(All powers of s) */

            assert(isExpressionAddendNull(pAddend));

        } /* End if(Special case of a null expression?) */

        /* .': Octave doesn't care but MATLAB requires row vectors. */
        print(stream, "].'");
    }

    print(stream, ";\n");

} /* End of printNamedExpressionAsMCode */




/**
 * Cancel one result term, i.e. a fraction of one numerator and the common denominator.
 *   @param ppCancelledExprNum
 * The pointer to the cancelled, denormalized numerator expression is placed into * \a
 * ppCancelledExprNum. The expressions is newly allocated and needs to be freed after use.
 *   @param ppCancelledExprDenom
 * The pointer to the cancelled, denormalized denominator expression is placed into * \a
 * ppCancelledExprDenom. The expressions is newly allocated and needs to be freed after use.
 *   @param pNExprNum
 * The pointer to the normalized numerator expression.
 *   @param pNExprDenom
 * The pointer to the normalized denominator expression.
 *   @param noConst
 * The total number of device constants, which are in use in the given system.
 */

static void cancelFraction( frq_frqDomExpression_t ** const ppCancelledExprNum
                          , frq_frqDomExpression_t ** const ppCancelledExprDenom
                          , const frq_normalizedFrqDomExpression_t * const pNExprNum
                          , const frq_normalizedFrqDomExpression_t * const pNExprDenom
                          , const unsigned int noConst
                          )
{
    frq_frqDomExpression_t *pNum, *pDenom;

    if(isNormalizedExpressionNull(pNExprDenom))
    {
        /* A denominator of null normally leads to the representation 1/0 for infinite.
           Only if the numerator is also null we say 0/0, meaning an undefined value. */
        if(isNormalizedExpressionNull(pNExprNum))
            pNum = expressionAddendNull();
        else
        {
            pNum = expressionAddendOne();
#if 0
            /* Weakness of concept: The "sign" of the numerator expression is not
               sufficient to distinguish between +inf and -inf. We don't know from which
               side of the null axis the denominator approached the null. Example: A
               constant current source feeds the conductance Y. The output voltage is
               1/Y*I0. If Y approaches null - realized by removing it from the circuit -
               then the voltage would rise to +inf. The solution found by the solver is 1/0
               and sign(1) seems to be the correct sign of inf. But actually, and depending
               on the order of nodes in the circuit net, the system could also figure out
               the solution as -1/-Y*I0 and for Y->0 (i.e. Y removed): -1/0 and sign(-1) is
               not the expected sign of inf. Just seeing D=0 doesn't permit to retrieve the
               correct sign of infinite.
                 @todo The best way out would be to introduce a representation of infinite,
               which doesn't pretend to express a sign, as 1/0 does. Write "inf" or use the
               tilted 8. This would mean a change of the result rendering routines. */
            if(estimateSignOfExpression(pNExprNum) < 0)
                pNum->factor = RAT_MINUS_ONE;
#endif
        }
        pDenom = expressionAddendNull();
    }
    else if(isNormalizedExpressionNull(pNExprNum))
    {
        pNum = expressionAddendNull();
        pDenom = expressionAddendOne();
    }
    else
    {
        /* Cancelling a fraction of normalized expressions is straightforward as the common
           multipliers are collected in the direct accessible factor. */
        const frq_frqDomExpressionAddend_t * const pComFactorNum = pNExprNum->pFactor
                                         , * const pComFactorDenom = pNExprDenom->pFactor;

        /* We shape an addend, which is multiplied with both, the numerator's and
           denominator's direct accessible factor. */
        frq_frqDomExpressionAddend_t *pFactorCancel = expressionAddendOne();

        /// @todo Here we probably have the operation with highest probability of overflow: The LCM as an intermediate result could be not representable in the shorter int range although the intended final result, the cancelled fraction could still be representable. Shape a test case to find out if we have an (avoidable) bottleneck here
        pFactorCancel->factor.n = rat_lcm(pComFactorNum->factor.d, pComFactorDenom->factor.d);
        pFactorCancel->factor.d = rat_gcd(pComFactorNum->factor.n, pComFactorDenom->factor.n);

        /* The numeric factor of a normalized expression has a sign such that the first
           term (i.e. highest power in s) of the remaining expression is positive. We will
           multiply both expressions with the found cancelling factor and want to have the
           positive sign for the first term of the resulting denominator. Consequently, the
           cancelling factor needs to have the same sign as the numeric factor of the
           normalized denominator expression. */
        if(rat_sign(pFactorCancel->factor) != rat_sign(pComFactorDenom->factor))
            pFactorCancel->factor = rat_neg(pFactorCancel->factor);

        unsigned int idxConst;
        for(idxConst=0; idxConst<noConst; ++idxConst)
        {
            /* To get rid of all negative individual powers and to guarantee the existence
               of the individual power of null in at least one term we need to extend the
               common factors with the negated minimum of the individual powers in both
               common factors. */
            if(pComFactorNum->powerOfConstAry[idxConst]
               < pComFactorDenom->powerOfConstAry[idxConst]
              )
            {
                pFactorCancel->powerOfConstAry[idxConst] =
                                                - pComFactorNum->powerOfConstAry[idxConst];
            }
            else
            {
                pFactorCancel->powerOfConstAry[idxConst] =
                                                - pComFactorDenom->powerOfConstAry[idxConst];
            }
        }

        /* Same handling of the individual powers of s as done before in the loop for the
           device constants. */
        if(pComFactorNum->powerOfS < pComFactorDenom->powerOfS)
            pFactorCancel->powerOfS = - pComFactorNum->powerOfS;
        else
            pFactorCancel->powerOfS = - pComFactorDenom->powerOfS;

        /* Cancelling the fraction can also mean that both remaining expressions in the
           normalized representation are identical. In which case we can completely cancel
           them. */
        const frq_frqDomExpression_t *pRemainingExprNum
                                   , *pRemainingExprDenom
                                   , *pOne = expressionAddendOne();

        if(isEqualExpressions(pNExprNum->pExpr, pNExprDenom->pExpr, noConst))
        {
            pRemainingExprNum   = pOne;
            pRemainingExprDenom = pOne;
        }
        else
        {
            /* Normal case: Use remaining expression as they are. */
            pRemainingExprNum   = pNExprNum->pExpr;
            pRemainingExprDenom = pNExprDenom->pExpr;
        }

        /* Multiply the common factor of the normalized expression with the found cancel
           factor. (This operation includes a copy of the addend, the normalized
           expressions of the solution must not be altered.) The final expression is got be
           denormalizing the remaining expression of the normalized representation with
           this product. Do this for numerator and denominator. */
        frq_frqDomExpressionAddend_t *pComFacCancelled = mulByAddendAndCpy( pComFactorNum
                                                                          , pFactorCancel
                                                                          , noConst
                                                                          );
        assert(pComFacCancelled->pNext == NULL);
        pNum = mulByAddendAndCpy(pRemainingExprNum, pComFacCancelled, noConst);
        freeExpressionAddend(pComFacCancelled);

        pComFacCancelled = mulByAddendAndCpy(pComFactorDenom, pFactorCancel, noConst);
        assert(pComFacCancelled->pNext == NULL);
        pDenom = mulByAddendAndCpy(pRemainingExprDenom, pComFacCancelled, noConst);
        freeExpressionAddend(pComFacCancelled);

        freeExpressionAddend(pFactorCancel);
        freeExpressionAddend(pOne);

    } /* End if(N and D are not null or trivial situation otherwise?) */

    /* In either case pNum and pDenom now hold the cancelled, denormalized expressions,
       which can be returned. Both expressions are newly allocated and need to be freed
       after use. */
    *ppCancelledExprNum = pNum;
    *ppCancelledExprDenom = pDenom;

} /* End of cancelFraction */




/**
 * Move a denormalized expression into the map required for result representation. "Move"
 * means, that the map takes the ownership of the passed expression. It'll be freed with
 * destruction of the map.
 *   @return
 * The index under which the expression is found in the map's internal, linear array is
 * returned. The most significant bit has a special meaning: It is set if the referenced
 * expression has to be taken minus one.
 *   @param pMap
 * The pointer to the map object. All needed storage is preallocated, no memory allocation
 * or free operations take place; the map needs to have enough storage space to complete
 * the operation.
 *   @param pExpr
 * The pointer to the expression or its heading addend.
 *   @param isUsedAsDenominator
 * Boolean flag if this is a numerator or denominator term.
 */

static unsigned int moveExprIntoMap( resultExpressionMap_t * const pMap
                                   , const frq_frqDomExpression_t * const pExpr
                                   , boolean isUsedAsDenominator
                                   )
{
    const unsigned int noConst = pMap->pSolution->pTableOfVars->noConstants;
    assert(pMap->noResExpr < pMap->maxNoResExpr);

    /* The main purpose of the map is to allow reuse of common terms. We do a linear
       iteration over all already stored expressions and look for identical ones. */
    unsigned int idxExpr;
    for(idxExpr=0; idxExpr<pMap->noResExpr; ++idxExpr)
    {
        boolean haveSameSign;
        if(isAbsEqualExpressions( &haveSameSign
                                , pMap->resExprAry[idxExpr].pExpr
                                , pExpr
                                , noConst
                                )
          )
        {
            /* The passed expression is no longer used, destroy object. */
            freeExpression(pExpr);

            /* Tag found, matching expression as denominator expression if so. */
            if(isUsedAsDenominator)
                pMap->resExprAry[idxExpr].isUsedAsDenom = true;

            unsigned int resultIdx = idxExpr;
            if(!haveSameSign)
                resultIdx |= RESULT_EXPR_REF_IS_NEGATED;
            return resultIdx;
        }
    } /* End for(All already stored expressions) */

    /* We got a new expression, put it at the end. Its name and the origina of this name
       are still unknown. */
    ++ pMap->noResExpr;
    pMap->resExprAry[idxExpr] = (resultExpression_t)
                                { .name = NULL
                                , .pExpr = pExpr
                                , .isUsedAsDenom = isUsedAsDenominator
                                , .origin = (resultExpressionOrigin_t)
                                            { .idxDependent = UINT_MAX
                                            , .idxIndependent = UINT_MAX
                                            , .isNumerator = false
                                            }
                                };
    return idxExpr;

} /* End of moveExprIntoMap */




/**
 * Create an expression map as used for sorted result representation. The map permits to
 * reuse common terms of the solution by giving them a name and referncing this name.
 * Sorting is needed to avoid forward references.
 *   @return
 * The pointer to the new map object is returned. It has been initialized with all
 * initially needed information. After use, it has to be destroyed with \a
 * deleteExpressionMap.
 *   @param pSolution
 * The pointer to the object representing the solution to be printed.
 */

static resultExpressionMap_t *createExpressionMap
                                        (const frq_freqDomainSolution_t * const pSolution)
{
    const tbv_tableOfVariables_t * const pTabOfVars = pSolution->pTableOfVars;
    const unsigned int noDependents = frq_getNoDependents(pSolution)
                     , noIndependents = frq_getNoIndependents(pSolution);

    resultExpressionMap_t *pExprMap = smalloc( sizeof(resultExpressionMap_t)
                                             , __FILE__
                                             , __LINE__
                                             );

    /* Prepare output: Cancel fractions and find common terms. */
    pExprMap->pSolution = frq_cloneByConstReference(pSolution);
    pExprMap->noResExpr = 0;
#ifdef DEBUG
    pExprMap->maxNoResExpr = 2*noDependents*noIndependents;
#endif
    pExprMap->resExprAry = smalloc( 2*noDependents*noIndependents*sizeof(resultExpression_t)
                                  , __FILE__
                                  , __LINE__
                                  );
    pExprMap->idxNumExprAry = unsignedInt_createMatrix(noDependents, noIndependents);
    pExprMap->idxDenomExprAry = unsignedInt_createMatrix(noDependents, noIndependents);

    /* Loop over all fractions: Cancel them and put cancelled numerator and denominator
       expression into the map. */
    unsigned int idxDependent, idxIndependent;
    for(idxDependent=0; idxDependent<noDependents; ++idxDependent)
    {
        for(idxIndependent=0; idxIndependent<noIndependents; ++idxIndependent)
        {
            frq_frqDomExpression_t *pCancelledExprNum
                                 , *pCancelledExprDenom;
            cancelFraction( &pCancelledExprNum
                          , &pCancelledExprDenom
                          , pSolution->numeratorAry[idxDependent][idxIndependent]
                          , pSolution->pDenominator
                          , pTabOfVars->noConstants
                          );
            pExprMap->idxNumExprAry[idxDependent][idxIndependent] =
                                            moveExprIntoMap( pExprMap
                                                           , pCancelledExprNum
                                                           , /* isUsedInDenominator */ false
                                                           );

            pExprMap->idxDenomExprAry[idxDependent][idxIndependent] =
                                            moveExprIntoMap( pExprMap
                                                           , pCancelledExprDenom
                                                           , /* isUsedInDenominator */ true
                                                           );
        }
    }

    LOG_DEBUG( _log
             , "createExpressionMap: The map contains %u expressions. %u expressions are"
               " reused"
             , pExprMap->noResExpr
             , 2*noDependents*noIndependents - pExprMap->noResExpr
             )

    return pExprMap;

} /* End of createExpressionMap */




/**
 * Destroy a result expression map after use. Counterpart of \a createExpressionMap.
 *   @param pExprMap
 * Pointer to the object to be destroyed.
 */

static void deleteExpressionMap(resultExpressionMap_t * const pExprMap)
{
    const frq_freqDomainSolution_t * const pSolution = pExprMap->pSolution;
    const unsigned int noDependents = frq_getNoDependents(pSolution)
                     , noIndependents = frq_getNoIndependents(pSolution);

    unsigned int idxExpr;
    for(idxExpr=0; idxExpr<pExprMap->noResExpr; ++idxExpr)
    {
        const resultExpression_t * const pResultExpr = &pExprMap->resExprAry[idxExpr];
        freeExpression(pResultExpr->pExpr);
    }

    free(pExprMap->resExprAry);
    unsignedInt_deleteMatrix(pExprMap->idxNumExprAry, noDependents, noIndependents);
    unsignedInt_deleteMatrix(pExprMap->idxDenomExprAry, noDependents, noIndependents);

    frq_deleteFreqDomainSolution(pSolution);

    free(pExprMap);

} /* End of deleteExpressionMap */





/**
 * A private method of the expression map: The origin, i.e. the name giving location of an
 * expression is set. From now on, the expression can be referenced by this name. The
 * assigned name is the name of a numerator or denominator expression as defined for the
 * contained solution object.\n
 *   The call is ignored if the expression should already have an origin. Once the origin
 * is defined it cannot be changed any more.
 *   @param pExprMap
 * The expression map object to be manipulated.
 *   @param pOrigin
 * The origin of the expression by reference as a set of coordinates into a solution
 * object.
 */

static void setNameOfExpression( resultExpressionMap_t * const pExprMap
                               , const resultExpressionOrigin_t * const pOrigin
                               )
{
    const unsigned int idxDependent = pOrigin->idxDependent;
    const unsigned int idxIndependent = pOrigin->idxIndependent;
    const boolean isNumerator = pOrigin->isNumerator;

    unsigned int idxExprInMap;
    if(isNumerator)
        idxExprInMap = pExprMap->idxNumExprAry[idxDependent][idxIndependent];
    else
        idxExprInMap = pExprMap->idxDenomExprAry[idxDependent][idxIndependent];

    /* The index of the expression has been merged with the sign bit. */
    idxExprInMap &= ~RESULT_EXPR_REF_IS_NEGATED;
    assert(idxExprInMap < pExprMap->noResExpr);

    if(pExprMap->resExprAry[idxExprInMap].name == NULL)
    {
        const frq_freqDomainSolution_t * const pSolution = pExprMap->pSolution;
        pExprMap->resExprAry[idxExprInMap].name =
                            isNumerator
                            ? getNameOfNumerator(pSolution, idxDependent, idxIndependent)
                            : getNameOfDenominator(pSolution, idxDependent, idxIndependent);
        pExprMap->resExprAry[idxExprInMap].origin = *pOrigin;
    }
} /* End of setNameOfExpression */




/**
 * A private method of the expression map: Set the names of all the expressions in the
 * expression map. The names are derived from the names of the dependents and independents.
 *   @param pExprMap
 * The expression map object.
 *   @param idxDependentAry
 * The order of rendering of dependents as figured out by void
 * determineOrderOfRendering(const resultExpressionMap_t * const, unsigned
 * int[]).
 */

static void setExpressionNames( resultExpressionMap_t * const pExprMap
                              , unsigned int idxDependentAry[]
                              )
{
    const frq_freqDomainSolution_t * const pSolution = pExprMap->pSolution;
    const unsigned int noDependents = frq_getNoDependents(pSolution)
                     , noIndependents = frq_getNoIndependents(pSolution);

    unsigned int u;
    resultExpressionOrigin_t origin;
    for(u=0; u<noDependents; ++u)
    {
        origin.idxDependent = idxDependentAry[u];
        assert(origin.idxDependent < frq_getNoDependents(pSolution));

        /* The denominator expressions of the dependent are handled all prior to its
           numerator expressions. */
        origin.isNumerator = false;
        for( origin.idxIndependent = 0
           ; origin.idxIndependent < noIndependents
           ; ++origin.idxIndependent
           )
        {
            setNameOfExpression(pExprMap, &origin);

        } /* End for(All of the dependent's denominator terms related to the knowns) */

        /* Now give a name to all the numerators' expression (which did not get a name
           yet). */
        origin.isNumerator = true;
        for( origin.idxIndependent = 0
           ; origin.idxIndependent < noIndependents
           ; ++origin.idxIndependent
           )
        {
            setNameOfExpression(pExprMap, &origin);

        } /* End for(All of the dependent's numerator terms related to the knowns) */

    } /* End for(All dependents) */

} /* End of setExpressionNames */




/**
 * A method of the expression map: The order is figured in which the results for the
 * dependent quantities should be printed in order to avoid forward references to
 * repeatedly used expressions.\n
 *   The order is returned as index array. The indexes mean the index of the dependent in the
 * solution object of type \a frq_freqDomainSolution_t, which the expression map had been
 * created for.\n
 *   After figuring out the right order of rendering the expressions, their names are set.
 *   @param pExprMap
 * The expression map object.
 *   @param idxDependentAry
 * The caller passes the (uninitialized) array to be filled by reference. The array has
 * room for as many indexes as dependents exist in the solution object the map had been
 * created from/for.\n
 *   The array indicates only the order of dependents. The order of independents is
 * implicitly defined to be their natural order of index. Furthermore and also implicitly
 * defined is the order of all denominator expressions of a dependent coming before any
 * numerator expression of that dependent.\n
 *   Only if all of these ordering aspects are considered during expression rendering then
 * forward references are safely avoided.
 */

static void determineOrderOfRendering( resultExpressionMap_t * const pExprMap
                                     , unsigned int idxDependentAry[]
                                     )
{
    const unsigned int noDependents = frq_getNoDependents(pExprMap->pSolution)
                     , noIndependents = frq_getNoIndependents(pExprMap->pSolution);
    boolean isNamedAry[pExprMap->noResExpr]
          , isReleasedAry[noDependents];

    /* Initialization: No expression is already named, no dependent is released for
       rendering. */
    memset(isNamedAry, /* value */ 0, sizeof(isNamedAry));
    memset(isReleasedAry, /* value */ 0, sizeof(isReleasedAry));

    /* We try to handle all dependents in their order of index. However, a dependent is
       released for rendering only if all its expressions are already named or open for
       naming. An expression is open for naming if it is solely used as numerator -
       denominators have the precedence in name giving. */

    /* Outer loop: We repeat looking for the next dependents to release as long as we
       released at least one in the preceding loop. */
    unsigned noReleasedDependents = 0;
    boolean doContinue;
    do
    {
        /* Continue the outer loop only if at least one dependent could be newly released
           for rendering. */
        doContinue = false;

        /* Inner loop: Test all not yet released dependents. */
        unsigned int idxDependent;
        for(idxDependent=0; idxDependent<noDependents; ++idxDependent)
        {
            if(isReleasedAry[idxDependent])
                continue;

            /* The test depends on the manipulated map of named expressions. However, the
               manipulation is established only after the test is found to be true.
               Therefore we need a temporary manipulation of the map. The map is copied to
               do so. */
            boolean isNamedAryTmp[pExprMap->noResExpr];
            memcpy(isNamedAryTmp, isNamedAry, sizeof(isNamedAryTmp));

            /* First we would name the expressions of the denominators of the result for
               the dependent under progress. Do this in the temporary map of named
               expressions. */
            unsigned int idxIndependent;
            for(idxIndependent=0; idxIndependent<noIndependents; ++idxIndependent)
            {
                unsigned int idxExpr = pExprMap->idxDenomExprAry[idxDependent][idxIndependent];
                idxExpr &= ~RESULT_EXPR_REF_IS_NEGATED;
                assert(idxExpr < pExprMap->noResExpr);
                isNamedAryTmp[idxExpr] = true;
            }

            /* Test of dependent: It is released if all required expressions are either
               named or not used by a denominator - in which case they could now be named
               by the numerator. */
            boolean isReleased = true;
            for( idxIndependent=0
               ; isReleased && idxIndependent<noIndependents
               ; ++idxIndependent
               )
            {
                unsigned int idxExpr = pExprMap->idxNumExprAry[idxDependent][idxIndependent];
                idxExpr &= ~RESULT_EXPR_REF_IS_NEGATED;
                assert(idxExpr < pExprMap->noResExpr);
                if(!isNamedAryTmp[idxExpr] &&  pExprMap->resExprAry[idxExpr].isUsedAsDenom)
                    isReleased = false;
            }

            if(isReleased)
            {
                /* We release another dependent, which can mean that some other, which
                   were bocked so far can be released now. We want to do another (outer)
                   loop of tests. */
                doContinue = true;

                /* The tested candidate can be rendered. The candidate is entered in the
                   result list and the temporary is-named-map becomes the actual one. */
                assert(noReleasedDependents < noDependents);
                idxDependentAry[noReleasedDependents++] = idxDependent;
                memcpy(isNamedAry, isNamedAryTmp, sizeof(isNamedAry));

                isReleasedAry[idxDependent] = true;
            }
        } /* End for(All not yet released dependents to be tested) */
    }
    while(doContinue);

    /* If there are still unreleased dependents then we have a situation where the desire
       to let denominators be dominant in name giving can't be fulfilled. It doesn't matter
       how to order these remaining dependents so we use their natural order by index. */
    unsigned int idxDependent;
    for(idxDependent=0; idxDependent<noDependents; ++idxDependent)
    {
        if(!isReleasedAry[idxDependent])
        {
            idxDependentAry[noReleasedDependents++] = idxDependent;

            /* No necessity to update isReleasedAry - it's no longer used. */
        }
    }
    assert(noReleasedDependents == noDependents);

    /* Having this order or expressions, we can safely set the names of the expressions
       such that denominators dominate and forward refernces are avoided. */
    setExpressionNames(pExprMap, idxDependentAry);

} /* End of determineOrderOfRendering */




/**
 * A method of the expression map: An expression is got from the map by index of dependent
 * and independent. The map needs to be filled and all expressions need to have a name,
 * i.e. \a determineOrderOfRendering must have been called prior to the first call of this
 * method.
 *   @param pExprMap
 * The expression map object to be read from.
 *   @param pName
 * The name of the expression is placed in * \a pName. The name has been derived from the
 * names of the dependents and independents in the preceding call of \a
 * determineOrderOfRendering. The name pattern is N_<nameDep>_<nameIndep> if a numerator
 * expression had been name giving or D_<nameDep>_<nameIndep> if a denominator expression
 * had been name giving.\n
 *   The idea of reusing expressions is to present the expression in all details, when the
 * very term is rendered, which was name giving to the expression. Consequently, the name
 * is not returned (i.e. * \a pName is set to NULL) if the triple \a idxDependent, \a
 * idxIndependent and \a isNumerator designates the name giving term of the solution.\n
 *   The same expression should be referenced by name at all other locations, where it
 * appears and hence, the name is returned.
 *   @param ppExpression
 * The pointer to the denormalized expression is placed in * \a ppExpression. This
 * expression has a normalized sign, i.e. the term of highest power in s has a positive
 * numeric factor. The sign of the actual result is returned as * \a pIsExpressionNegated.
 *   @param pIsExpressionNegated
 * If * \a pIsExpressionNegated is true then the expression ** \a ppExpression needs to be
 * multiplied by -1 prior to further processing.
 *   @param idxDependent
 * The index of the dependent quantity.
 *   @param idxIndependent
 * The index of the independent quantity.
 *   @param isNumerator
 * Pass \a true if the numerator expression is requested and \a false if the denominator is
 * requested.
 */

static void getExpression( const resultExpressionMap_t * const pExprMap
                         , const char ** const pName
                         , boolean * const pIsExpressionNegated
                         , const frq_frqDomExpression_t ** const ppExpression
                         , unsigned int idxDependent
                         , unsigned int idxIndependent
                         , boolean isNumerator
                         )
{
    unsigned int idxExprInMap;
    if(isNumerator)
        idxExprInMap = pExprMap->idxNumExprAry[idxDependent][idxIndependent];
    else
        idxExprInMap = pExprMap->idxDenomExprAry[idxDependent][idxIndependent];

    /* The index of the expression has been merged with the sign bit. */
    *pIsExpressionNegated = (idxExprInMap & RESULT_EXPR_REF_IS_NEGATED) != 0;
    idxExprInMap &= ~RESULT_EXPR_REF_IS_NEGATED;
    assert(idxExprInMap < pExprMap->noResExpr);

    /* Expression reusage: The expression is presented as such when the name giving term is
       rendered. At all other locations, where the same expression is needed, it can be
       referenced by name. The function's ouput variable name is set accordingly
       conditionallly. */
    const resultExpressionOrigin_t * const pOrigin =
                                            &pExprMap->resExprAry[idxExprInMap].origin;
    if(pOrigin->idxDependent == idxDependent  &&  pOrigin->idxIndependent == idxIndependent
       &&  pOrigin->isNumerator == isNumerator
      )
    {
        /* For the name giving expression the name must not be used as a reference. */
        *pName = NULL;
    }
    else
    {
        *pName = pExprMap->resExprAry[idxExprInMap].name;
        assert(*pName != NULL);
    }

    /* The expression as such is returned even if it could be referenced by name: For
       trivial expressions like 1 or 0 it might be more convenient to present them as such
       rather then to reference another location, where it had already appeared. */
    *ppExpression = pExprMap->resExprAry[idxExprInMap].pExpr;

} /* End of getExpression */



/**
 * Write the solution into a stream in a human readable form.
 *   @return
 * The result representation uses math operations, which can overflow. The presented result
 * is valid only if this function returns \a true. A specific error message has been
 * written to the log if \a false is returned.
 *   @param pSolution
 * The pointer to the object representing the solution to be printed. The object is the result
 * of a successful call of frq_freqDomainSolution_t *frq_createFreqDomainSolution(const
 * sol_solution_t * const, unsigned int idxResult).
 *   @param stream
 * The stream (as provided by the stdio lib) to write to.
 *   @param asOctaveCode
 * The solution is printed either in human readable or as Octave script code.
 *   @param printMargin
 * Line wrapping takes place when the margin is exceeded but the currently printed addend
 * of the expression is still not completed. Checking the margin only after completing an
 * addend means that the printed lines can become significantly longer than the margin
 * says. So take a rather small value for the margin.
 */

static boolean printSolution( const frq_freqDomainSolution_t * const pSolution
                            , FILE * const stream
                            , boolean asOctaveCode
                            , const unsigned int printMargin
                            )
{
    /* Safe error recognition and location requires that the global error flag is reset on
       function entry. */
    assert(!rat_getError());

    /* The human readable text parts are preceded by a comment character if we produce
       Octave script code. */
    const char * const tabStringText = asOctaveCode? "% ": "";

    /* Print a title line. */
    const char * const strIsBodePlot = frq_getIsBodePlot(pSolution)? " (Bode plot)": ""
             , * const strFormat = pSolution->idxResult >= 0
                                   ? "%sUser-defined result %s%s:\n"
                                   : "%sResult %s%s in the frequency domain:\n";
    print(stream, strFormat, tabStringText, frq_getResultName(pSolution), strIsBodePlot);

    const tbv_tableOfVariables_t * const pTabOfVars = pSolution->pTableOfVars;
    const unsigned int noDependents = frq_getNoDependents(pSolution)
                     , noIndependents = frq_getNoIndependents(pSolution);

    resultExpressionMap_t * const pExprMap = createExpressionMap(pSolution);

    /* Figure out in which order to print the results for the dependents so that we safely
       avoid forward references to repeatedly used expressions. */
    unsigned int idxDependentAry[noDependents];
    determineOrderOfRendering(pExprMap, idxDependentAry);

    /* The line indentation for printing the distinct numerator and denominator
       expressions. */
    char tabStringExpr[128];
    getBlankTabString( tabStringExpr
                     , sizeof(tabStringExpr)
                     , /* existingTabString */ tabStringText
                     , /* additionalIndentation */ 4
                     );

    /* The output looks a bit different for Bode plots. */
    const boolean isBodePlot = frq_getIsBodePlot(pSolution);

    unsigned int u;
    for(u=0; u<noDependents; ++u)
    {
        unsigned int idxDependent = idxDependentAry[u];
        assert(idxDependent < frq_getNoDependents(pSolution));

        const char * const nameDependent = frq_getNameOfDependent(pSolution, idxDependent)
                 , * nameIndependent = isBodePlot? frq_getNameOfIndependent(pSolution, 0): ""
                 , * const titleString = isBodePlot
                                         ? "%sThe dependency of %s on %s:"
                                           "\n%s  %s(s) = "
                                         : "%sThe solution for unknown %s%s:"
                                           "\n%s  %s(s) = ";
        if(asOctaveCode)
            print(stream, "\n");
        print( stream
             , titleString
             , tabStringText
             , nameDependent
             , nameIndependent
             , tabStringText
             , nameDependent
             );

        /* In case of a full result the independents are the knowns of the LES. */
        const signed int indentDepth = strlen(tabStringText) + strlen(nameDependent)
                                       + sizeof("  (s) = ") - 1;
        unsigned int idxIndependent;
        for(idxIndependent=0; idxIndependent<noIndependents; ++idxIndependent)
        {
            nameIndependent = frq_getNameOfIndependent(pSolution, idxIndependent);
            print( stream
                 , "%s%s(s)/%s(s) * %s(s)"
                 , idxIndependent > 0? "+ ": ""
                 , getNameOfNumerator(pSolution, idxDependent, idxIndependent)
                 , getNameOfDenominator(pSolution, idxDependent, idxIndependent)
                 , nameIndependent
                 );

            /* For more than one independent: Break line after each term. */
            if(idxIndependent+1 < noIndependents)
                print(stream, "\n%-*s", indentDepth, tabStringText);
        }
        print(stream, ", with\n");

        for(idxIndependent=0; idxIndependent<noIndependents; ++idxIndependent)
        {
            /* The human readable output uses the natural order numerator before
               denominator, although this can lead to forward references. These forward
               references (if any) only go one expression ahead and are therefore
               acceptable. */

            /* Retrieve the numerator expression from the map. */
            const char *nameOfExpr;
            boolean isExpressionNegated;
            const frq_frqDomExpression_t *pExpr;
            getExpression( pExprMap
                         , &nameOfExpr
                         , &isExpressionNegated
                         , &pExpr
                         , idxDependent
                         , idxIndependent
                         , /* isNumerator */ true
                         );


            /* Print the expression. */
            const char *LHS = getNameOfNumerator(pSolution, idxDependent, idxIndependent);
            printNamedExpression( stream
                                , LHS
                                , nameOfExpr
                                , pExpr
                                , isExpressionNegated
                                , pTabOfVars
                                , printMargin
                                , tabStringExpr
                                );

            /* Do the same for the denominator. */
            getExpression( pExprMap
                         , &nameOfExpr
                         , &isExpressionNegated
                         , &pExpr
                         , idxDependent
                         , idxIndependent
                         , /* isNumerator */ false
                         );

            /* Print the expression. */
            LHS = getNameOfDenominator(pSolution, idxDependent, idxIndependent);
            printNamedExpression( stream
                                , LHS
                                , nameOfExpr
                                , pExpr
                                , isExpressionNegated
                                , pTabOfVars
                                , printMargin
                                , tabStringExpr
                                );
        } /* End for(All of the dependent's terms related to the knowns) */

        /* If we generate Octave script code: Print the expressions a second time as an
           assignment operation. */
        if(asOctaveCode)
        {
            /* To strictly avoid forward references in the Octave code we need to write all
               the denominators prior to the numerators. Here we have the denominators. */
            for(idxIndependent=0; idxIndependent<noIndependents; ++idxIndependent)
            {
                /* Retrieve the denominator expression from the map. */
                const char *nameOfExpr;
                boolean isExpressionNegated;
                const frq_frqDomExpression_t *pExpr;
                getExpression( pExprMap
                             , &nameOfExpr
                             , &isExpressionNegated
                             , &pExpr
                             , idxDependent
                             , idxIndependent
                             , /* isNumerator */ false
                             );

                /* Print the expression. */
                const char * const LHS = getNameOfDenominator( pSolution
                                                             , idxDependent
                                                             , idxIndependent
                                                             );
                printNamedExpressionAsMCode( stream
                                           , LHS
                                           , nameOfExpr
                                           , pExpr
                                           , isExpressionNegated
                                           , pTabOfVars
                                           , printMargin
                                           );

                /* Write an error statement if the denominator is null: The Octave
                   functions don't respond with useful error feedback in this case. */
                if(isExpressionAddendNull(pExpr))
                {
                    print( stream
                         , "error(['Denominator expression %s is null. The transfer"
                           " function is' ...\n"
                           "       ' undefined and no plots can be generated. Please"
                           " check your circuit'] ...\n"
                           "     );\n"
                         , LHS
                         );
                }
            } /* End for(All of the dependent's denominators related to the knowns) */

            /* Now print all numerators as Octave script code. */
            for(idxIndependent=0; idxIndependent<noIndependents; ++idxIndependent)
            {
                /* Retrieve the numerator expression from the map. */
                const char *nameOfExpr;
                boolean isExpressionNegated;
                const frq_frqDomExpression_t *pExpr;
                getExpression( pExprMap
                             , &nameOfExpr
                             , &isExpressionNegated
                             , &pExpr
                             , idxDependent
                             , idxIndependent
                             , /* isNumerator */ true
                             );

                /* Print the expression. */
                const char * const LHS = getNameOfNumerator( pSolution
                                                           , idxDependent
                                                           , idxIndependent
                                                           );
                printNamedExpressionAsMCode( stream
                                           , LHS
                                           , nameOfExpr
                                           , pExpr
                                           , isExpressionNegated
                                           , pTabOfVars
                                           , printMargin
                                           );
            } /* End for(All of the dependent's numerators related to the knowns) */

        } /* End if(Output of expressions as Octave code needed?) */

    } /* End for(All dependents) */

    /* Free memory of data structures used temporary to order the result terms. */
    deleteExpressionMap(pExprMap);

    fflush(stream);

    /* Check global arithmetic error flag. An error message has already been printed to the
       log in case. */
    boolean success = rat_getError() == false;
    rat_clearError();

    return success;

} /* End of printSolution */




/**
 * Initialize the module at application startup.
 *   @param hLogger
 * This module will use the passed logger object for all reporting during application life
 * time. It must be a real object, LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT is not permitted.
 *   @remark
 * Do not forget to call the counterpart at application end.
 *   @remark
 * This module depends on the other module log_logger. It needs to be initialized after
 * this other module.
 *   @remark Using this function is not an option but a must. You need to call it
 * prior to any other call of this module and prior to accessing any of its global data
 * objects.
 *   @see void frq_shutdownModule()
 */

void frq_initModule(log_hLogger_t hLogger)
{
    assert(hLogger != LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT);
    _log = log_cloneByReference(hLogger);

    /* Initialize the global heap of addends. */
    _hHeapOfAddends = mem_createHeap( _log
                                    , /* Name */ "Addend of frequency domain expression"
                                    , sizeof(frq_frqDomExpressionAddend_t)
                                    , /* initialHeapSize */     100
                                    , /* allocationBlockSize */ 500
                                    );
#ifdef DEBUG
   frq_frqDomExpressionAddend_t dummyObj;
   assert((char*)&dummyObj.pNext == (char*)&dummyObj + MEM_OFFSET_OF_LINK_POINTER
          &&  sizeof(dummyObj.pNext) == MEM_SIZE_OF_LINK_POINTER
         );
#endif
#ifdef  DEBUG
    /* The DEBUG compilation counts all references to all created objects. */
    _noRefsToSolutionObjects = 0;
    _noRefsToExprObjects = 0;
#endif
#ifdef  DEBUG
    /* Check if patch of snprintf is either not required or properly installed. */
    char buf[3] = {[2] = '\0'};
    snprintf(buf, 2, "%s World", "Hello");
    assert(strlen(buf) == 1);
#endif
} /* End of frq_initModule */




/**
 * Do all cleanup after use of the module, which is required to avoid memory leaks, orphaned
 * handles, etc.
 */

void frq_shutdownModule()
{
#ifdef  DEBUG
    /* The DEBUG compilation looks for still allocated objects in order to detect memory
       leaks. */
    if(_noRefsToSolutionObjects != 0)
    {
        fprintf( stderr
               , "frq_shutdownModule: %u references to objects of type"
                 " frq_freqDomainSolution_t have not been discarded at application"
                 " shutdown. There are probable memory leaks\n"
               , _noRefsToSolutionObjects
               );
    }
    if(_noRefsToExprObjects != 0)
    {
        fprintf( stderr
               , "frq_shutdownModule: %u references to objects of type"
                 " frq_normalizedFrqDomExpression_t have not been discarded at application"
                 " shutdown. There are probable memory leaks\n"
               , _noRefsToExprObjects
               );
    }
#endif

    /* The DEBUG compilation looks for still allocated objects in order to detect memory
       leaks. */
#ifdef DEBUG
    mem_deleteHeap(_hHeapOfAddends, /* warnIfUnfreedMem */ true);
#else
    mem_deleteHeap(_hHeapOfAddends, /* warnIfUnfreedMem */ false);
#endif
    _hHeapOfAddends = MEM_HANDLE_INVALID_HEAP;

    /* Discard reference to the logger object. */
    log_deleteLogger(_log);
    _log = LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT;

} /* End of frq_shutdownModule */




/**
 * Create an object that a user defined result. The user defined result is a set of complex
 * expressions, that describe the solution for a sub-set of unknowns of the LES and/or
 * user defined voltages in the frequency domain.
 *   @return
 * \a true if a valid object could be created, \a false otherwise. An error report has been
 * written to the global application log if \a false should be returned.
 *   @param ppFrqDomSolution
 * A pointer to the new object is returned in * \a pFrqDomSolution. Some memory has been
 * allocated on the heap; therefore the object needs to be deleted again after usage.\n
 *   In case of an error a null object, FRQ_NULL_SOLUTION, is returned.
 *   @param pAlgebraicSolution
 * The frequency domain solution is derived from an algebraic solution as figured out by the
 * symbolic solver. Please, refer to boolean sol_createSolution(log_hLogger_t, const
 * sol_solution_t ** const, les_linearEquationSystem_t * const). This object contains a
 * solution for all unknowns of the LES and for all user-defined voltages.
 *   @param idxResult
 * The index of the user defined result, the solution is requested for. If a negative value
 * is passed then a full result for all unknowns of the LES and all user defined voltages
 * is returned.
 *   @see void frq_deleteFreqDomainSolution(frq_freqDomainSolution_t * const)
 */

boolean frq_createFreqDomainSolution( const frq_freqDomainSolution_t * * const ppFrqDomSolution
                                    , const sol_solution_t * const pAlgebraicSolution
                                    , signed int idxResult
                                    )
{
    assert(_log != LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT);

    /* Safe error recognition and location requires that the global error flag is reset on
       function entry. */
    boolean success = !rat_getError();
    assert(success);

    frq_freqDomainSolution_t *pRes = smalloc( sizeof(frq_freqDomainSolution_t)
                                            , __FILE__
                                            , __LINE__
                                            );

    /* The result of the constructor is the first reference to the new object. */
    pRes->noReferencesToThis = 1;
#ifdef DEBUG
    ++ _noRefsToSolutionObjects;
#endif

    /* Clone the table of variables. It is required to give some meaning to the result
       terms. */
    const tbv_tableOfVariables_t * const pTableOfVars =
                                  tbv_cloneByConstReference(pAlgebraicSolution->pTableOfVars);
    pRes->pTableOfVars = pTableOfVars;
    pRes->idxResult = idxResult;
    if(idxResult >= 0)
    {
        /* Set the name of the requested result. */
        assert((unsigned)idxResult < pTableOfVars->pCircuitNetList->noResultDefs);
        pRes->name = pTableOfVars->pCircuitNetList->resultDefAry[idxResult].name;
    }
    else
        pRes->name = "allDependents";

    /* After setting pTableOfVars and idxResult, the member function for requesting the
       number of dependents and independents may be called - ven for this half-way
       completed object. */
    const unsigned int noDependents   = frq_getNoDependents(pRes)
                     , noIndependents = frq_getNoIndependents(pRes);

    if(noDependents > 0  &&  noIndependents > 0)
    {
        /* Allocate memory for all terms of the result. There's one term per known and
           per unknown a solution is demanded for. Such a term has an individual
           numerator and all have the same common denominator.
             The denominator and all elements of the matrix are initialized to the null
           expression. After this step, the standard deconstructor can be called on the
           half-way completed object (in case of errors).
             User defined voltages of interest are handled like the original unknowns
           of the LES. */
        pRes->numeratorAry = normalizedFrqDomExpression_createMatrix
                                                        ( /* noRows */ noDependents
                                                        , /* noCols */ noIndependents
                                                        );
        pRes->pDenominator = normalizedExpressionNull();

        /* A terms, numerators and denominators, are frequency domain expressions, which
           are used at different locations in different contexts. For sake of consistency
           and since it's even crucial for some algorithms, we centralize the naming of
           these expressions in the solution object. This is redundent storage as all names
           are entirely derived from other information held in the solution object.
             The terms' denominators get individual names; due to individual canceling of
           the fractions they won't stay all the same. */
        pRes->numeratorNameAry = string_createMatrix( /* noRows */ noDependents
                                                    , /* noCols */ noIndependents
                                                    );
        pRes->denominatorNameAry = string_createMatrix( /* noRows */ noDependents
                                                      , /* noCols */ noIndependents
                                                      );
        initializeNameArys(pRes);


        /* Either a specific user-defined result is prepared or a full result for all
           dependent quantities. */
        if(idxResult >= 0)
        {
            /* Get a reference to the requested result. This object holds the solution for
               a user defined selection of unknowns. */
            assert((unsigned)idxResult < pTableOfVars->pCircuitNetList->noResultDefs);
            const pci_resultDef_t * const pResultDef = &pTableOfVars
                                                        ->pCircuitNetList
                                                          ->resultDefAry[idxResult];

            /* There should be at least one dependent quantity be defined for a result.
               Otherwise we have a not recognized parse error. */
            assert(pResultDef->noDependents > 0);

            /* If the name of the independent result quantity is NULL then a full result is
               demanded, which depends on all knows of the system. */
            if(pResultDef->independentName == NULL)
            {
                success = transformExpression( &pRes->pDenominator
                                             , pAlgebraicSolution->pDeterminant
                                             , pTableOfVars
                                             );

                /* Loop over the set of dependents of this frq solution. */
                unsigned int idxDependent;
                for(idxDependent=0; success && idxDependent<noDependents; ++idxDependent)
                {
                    const char * const nameOfUnknown =
                                            pResultDef->dependentNameAry[idxDependent];
                    signed int idxSolution;
                    unsigned int noMatches = sol_findName( pAlgebraicSolution
                                                         , &idxSolution
                                                         , /* pIdxUnknown       */ NULL
                                                         , /* pIdxUserDefVoltage*/ NULL
                                                         , /* pIdxKnown         */ NULL
                                                         , nameOfUnknown
                                                         , /* doErrorReporting */ true
                                                         );

                    /* The names of the unknows a solution is requested for are taken from
                       the input as are but have not been validated so far. A negative
                       return value is not an internal error but needs to be properly
                       handled with meaningful feedback. */
                    if(noMatches == 1  &&  idxSolution >= 0)
                    {
                        assert((unsigned)idxSolution < sol_getNoDependents(pAlgebraicSolution)
                               && pAlgebraicSolution->pIsDependentAvailableAry[idxSolution]
                              );

                        /* We have one result term for each known of the LES. */
                        unsigned int idxKnown;
                        for(idxKnown=0; success && idxKnown<noIndependents; ++idxKnown)
                        {
                            /* Transform the numerator of the result term. */
                            success = transformExpression
                                                ( &pRes->numeratorAry[idxDependent][idxKnown]
                                                , pAlgebraicSolution
                                                  ->numeratorAry[idxSolution][idxKnown]
                                                , pTableOfVars
                                                );
                        } /* End for(All terms of the solution for a single unknown) */
                    }
                    else
                    {
                        success = false;
                        
                        /* The search succeeded sucessfully and unambiguous but the found
                           quantity is an input quantity. */
                        if(noMatches == 1)
                        {
                            LOG_ERROR( _log
                                     , "A full result has been requested for quantity %s."
                                       " This is invalid as %s is a known quantity of the"
                                       " system (i.e. a system input)"
                                     , nameOfUnknown
                                     , nameOfUnknown
                                     )                            
                        }
                        /* else: The problem has been reported by sol_findName. */

                    } /* End if(Requested unknow is defined?) */

                } /* End for(All dependents of the frq solution) */
            }
            else
            {
                /* The requested result is the dependency of one quantity on one other one.
                   The advantage of this (basically restricted) result form is that inverse
                   transfer functions can be requested. Both quantities can be knowns or
                   unknowns, except for both being knowns at a time. Both quantities being
                   unknowns is restricted to systems with a single known. */

                /* Get access to the in- and output of the Bode plot, which we don't know a
                   priori whether it is a known or an unknown of the original LES. */
                signed int idxSolutionDependent
                         , idxKnownDependent
                         , idxSolutionIndependent
                         , idxKnownIndependent;
                assert(pResultDef->noDependents == 1);
                success = sol_findName( pAlgebraicSolution
                                      , &idxSolutionDependent
                                      , /* pIdxUnknown */ NULL
                                      , /* pIdxUserDefVoltage */ NULL
                                      , &idxKnownDependent
                                      , /* nameOfQuantity */ pResultDef->dependentNameAry[0]
                                      , /* doErrorReporting */ true
                                      ) == 1
                          &&  sol_findName( pAlgebraicSolution
                                          , &idxSolutionIndependent
                                          , /* pIdxUnknown */ NULL
                                          , /* pIdxUserDefVoltage */ NULL
                                          , &idxKnownIndependent
                                          , /* nameOfQuantity */ pResultDef->independentName
                                          , /* doErrorReporting */ true
                                          ) == 1;

                /* Figure out, which situation we have. */
                const coe_coef_t *pAlgebraicNum, *pAlgebraicDenom;

                if(success)
                {
                    if(idxSolutionDependent >= 0  &&  idxKnownIndependent >= 0)
                    {
                        /* This is the most usual situation: A user-defined voltage or
                           original unknown of the LES is plotted as function of a known
                           (i.e. known, given input voltage). */
                        assert(pAlgebraicSolution
                               ->pIsDependentAvailableAry[idxSolutionDependent]
                              );
                        pAlgebraicNum =
                                    pAlgebraicSolution
                                    ->numeratorAry[idxSolutionDependent][idxKnownIndependent];
                        pAlgebraicDenom = pAlgebraicSolution->pDeterminant;
                    }
                    else if(idxKnownDependent >= 0  &&  idxSolutionIndependent >= 0)
                    {
                        /* The inverse situation: A system input is plotted as function of
                           an actual system output. We have the inverse transfer function. */
                        pAlgebraicNum = pAlgebraicSolution->pDeterminant;
                        assert(pAlgebraicSolution
                               ->pIsDependentAvailableAry[idxSolutionIndependent]
                              );
                        pAlgebraicDenom =
                                    pAlgebraicSolution
                                    ->numeratorAry[idxSolutionIndependent][idxKnownDependent];
                    }
                    else if(idxSolutionDependent >= 0  &&  idxSolutionIndependent >= 0)
                    {
                        /* The user demands to plot two system outputs as function of each
                           other. This is defined only for systems with a single
                           independent. */
                        if(sol_getNoIndependents(pAlgebraicSolution) == 1)
                        {
                            assert(pAlgebraicSolution
                                   ->pIsDependentAvailableAry[idxSolutionDependent]
                                   && pAlgebraicSolution
                                      ->pIsDependentAvailableAry[idxSolutionIndependent]
                                  );
                            pAlgebraicNum = pAlgebraicSolution
                                            ->numeratorAry[idxSolutionDependent][0];
                            pAlgebraicDenom = pAlgebraicSolution
                                              ->numeratorAry[idxSolutionIndependent][0];
                        }
                        else
                        {
                            success = false;
                            LOG_ERROR( _log
                                     , "The dependent quantity %s can't be plotted as function"
                                       " of the other dependent quantity %s. Two dependents"
                                       " can be a function of each other only in the case of a"
                                       " single independent quantity. The given system has"
                                       " however %d inputs"
                                     , sol_getNameOfDependent( pAlgebraicSolution
                                                             , idxSolutionDependent
                                                             )
                                     , sol_getNameOfDependent( pAlgebraicSolution
                                                             , idxSolutionIndependent
                                                             )
                                     , sol_getNoIndependents(pAlgebraicSolution)
                                     )
                        }
                    }
                    else
                    {
                        assert(idxKnownIndependent >= 0  &&  idxKnownDependent >= 0);

                        /* No way to plot a pair of independent quantities as function of
                           each other. */
                        success = false;
                        LOG_ERROR( _log
                                 , "The independent quantity %s can't be plotted as function"
                                   " of the other independent quantity %s. Two independents"
                                   " or system inputs must not be specified for"
                                   " a Bode plot result"
                                 , sol_getNameOfIndependent( pAlgebraicSolution
                                                           , idxKnownIndependent
                                                           )
                                 , sol_getNameOfIndependent( pAlgebraicSolution
                                                           , idxKnownDependent
                                                           )
                                 , sol_getNoIndependents(pAlgebraicSolution)
                                 )
                    } /* End if/else if(Who depends on whom?) */
                } /* End if(Do the referenced dependent and independent exist?) */

                if(success)
                {
                    success = transformExpression( &pRes->pDenominator
                                                 , pAlgebraicDenom
                                                 , pTableOfVars
                                                 )
                              && transformExpression( &pRes->numeratorAry[0][0]
                                                    , pAlgebraicNum
                                                    , pTableOfVars
                                                    );
                }
            } /* End if(Full result term or a dependency one on one?) */
        }
        else
        {
            /* A negative result index means to get a solution for all dependents. These are
               the unknowns of the LES plus the user-defined voltages. */
            assert(noDependents == sol_getNoDependents(pAlgebraicSolution));

            success = transformExpression( &pRes->pDenominator
                                         , pAlgebraicSolution->pDeterminant
                                         , pTableOfVars
                                         );

            /* Loop over all dependents, here all dependents of both, the algebraic
               solution and the frq object. */
            unsigned int idxDependent;
            for(idxDependent=0; success && idxDependent<noDependents; ++idxDependent)
            {
                /* We have one result term for each known of the LES. */
                unsigned int idxKnown;
                for(idxKnown=0; success && idxKnown<noIndependents; ++idxKnown)
                {
                    /* Transform the numerator of the result term. */
                    assert(pAlgebraicSolution->pIsDependentAvailableAry[idxDependent]);
                    success = transformExpression( &pRes->numeratorAry[idxDependent][idxKnown]
                                                 , pAlgebraicSolution
                                                   ->numeratorAry[idxDependent][idxKnown]
                                                 , pTableOfVars
                                                 );
                } /* End for(All terms of the solution of one dependent of the frq object) */

            } /* End for(All unknowns) */

        } /* End if(User-defined result or a solution for  all dependents?) */

        /* Check global arithmetic error flag. An error message has already been printed to the
           log in case. */
        if(rat_getError())
        {
            success = false;
            rat_clearError();
        }
    }
    else
    {
        /* Abnormal solution: noDep*noIndeps == 0. */
        success = false;
        pRes->numeratorAry = NULL;
        pRes->pDenominator = NULL;
        pRes->numeratorNameAry = NULL;
        pRes->denominatorNameAry = NULL;

        /* Actually, noDep == 0 should never happen. We use this to give more precise
           feedback to the user. */
        assert(noDependents > 0);
        LOG_ERROR( _log
                 , "The system has an abnormal solution with no independents. All"
                   " dependents are null. No transfer function is figured out and no"
                   " Octave script is generated. Please, consider to use at least one"
                   " constant source in your circuit"
                 )
    }

    /* Normalize the representation of the terms: Avoid negative powers. */
    if(success)
        *ppFrqDomSolution = pRes;
    else
    {
        /* frq_getIsBodePlot: Is safely usable for half-way completed objects. */
        LOG_ERROR( _log
                 , "User-defined result %s %scan't be computed due to"
                   " previous errors"
                 , pRes->name
                 , frq_getIsBodePlot(pRes)? "(Bode plot) ": ""
                 );

        /* Free half-way completed or invalid object again. */
        frq_deleteFreqDomainSolution((const frq_freqDomainSolution_t *)pRes);
        *ppFrqDomSolution = FRQ_NULL_SOLUTION;
    }

    return success;

} /* End of frq_createFreqDomainSolution */





/**
 * Request another reference to an existing object. The new reference is counted internally
 * for later and safe control of the delete operation.\n
 *   Any requested reference needs to be freed with frq_deleteFreqDomainSolution after use.
 *   @return
 * A copy of the passed pointer \a pExistingObj is returned.
 *   @param pExistingObj
 * Pointer to the object to be cloned by reference.
 *   @see const frq_freqDomainSolution_t *frq_createFreqDomainSolution()
 *   @see frq_deleteFreqDomainSolution(const frq_freqDomainSolution_t * const)
 */

frq_freqDomainSolution_t *frq_cloneByReference(frq_freqDomainSolution_t * const pExistingObj)
{
    assert(pExistingObj != FRQ_NULL_SOLUTION);
    ++ pExistingObj->noReferencesToThis;

#ifdef DEBUG
    ++ _noRefsToSolutionObjects;
#endif

    return pExistingObj;

} /* End of frq_cloneByReference */





/**
 * Request another reference to an existing constant object. The new reference is counted
 * internally for later and safe control of the delete operation.\n
 *   Any requested reference needs to be freed with frq_deleteFreqDomainSolution after use.
 *   @return
 * A read-only copy of the passed pointer \a pExistingObj is returned.
 *   @param pExistingObj
 * Pointer to the (read-only) object to be cloned by reference.
 *   @see const frq_freqDomainSolution_t *frq_createFreqDomainSolution()
 *   @see frq_deleteFreqDomainSolution(const frq_freqDomainSolution_t * const)
 *   @remark
 * The method is a kind of work around the somewhat unsatisfying concept of const data in
 * C. Semantically, we make a copy of a never changed object to provide read access to
 * another client. Actually this requires a write operation on the reference counter. A
 * second problem is the deletion of such a copy; eventually it needs to call the free
 * operation, which is not permitted just like that for const objects. We circumvent these
 * problems by using explicit casts from const to modifiable. To keep these ugly operations
 * local, we hide them in this function's and the destructor's implementation. Outside the
 * copy operator the returned object can safely be used as read-only.\n
 *   Actually, the operation of this function is exactly identical to its non constant
 * counterpart.
 */

const frq_freqDomainSolution_t *frq_cloneByConstReference
                                        (const frq_freqDomainSolution_t * const pExistingObj)
{
    return frq_cloneByReference((frq_freqDomainSolution_t*)pExistingObj);

} /* End of frq_cloneByConstReference */





/**
 * Delete a reference to an existing object after use. If there are no references left then
 * the object itself is deleted, all memory is freed.
 *   @param pConstFreqDomainSolution
 * Pointer to the object to be deleted. No action if this is the NULL pointer.
 *   @see frq_freqDomainSolution_t *frq_createFreqDomainSolution(unsigned int, unsigned int,
 * unsigned int)
 *   @see frq_freqDomainSolution_t *frq_cloneByReference(frq_freqDomainSolution_t *)
 *   @remark
 * Regardless of the destructive operation the object is declared read-only in order to
 * support deletion of read-only copies of references to existing objects.
 */

void frq_deleteFreqDomainSolution
                        (const frq_freqDomainSolution_t * const pConstFreqDomainSolution)
{
    if(pConstFreqDomainSolution == FRQ_NULL_SOLUTION)
        return;

    /* See discussion of const declaration at frq_cloneByConstReference. The idea of const
       objects requires that we hurt the type qualifier locally here. */
    frq_freqDomainSolution_t * const pFreqDomainSolution =
                                        (frq_freqDomainSolution_t*)pConstFreqDomainSolution;

    /* Deletion takes place only if there are no known other references to this object. */
    assert(pFreqDomainSolution->noReferencesToThis >= 1);
    if(--pFreqDomainSolution->noReferencesToThis == 0)
    {
        const unsigned int noDependents   = frq_getNoDependents(pConstFreqDomainSolution)
                         , noIndependents = frq_getNoIndependents(pConstFreqDomainSolution);

        /* Free the array of numerators of the result terms. */
        normalizedFrqDomExpression_deleteMatrix( pFreqDomainSolution->numeratorAry
                                               , /* noRows */ noDependents
                                               , /* noCols */ noIndependents
                                               );

        /* Free the common denominator. */
        freeNormalizedExpression(pFreqDomainSolution->pDenominator);

        /* Free the name strings of the numerator and denominator expressions. */
        string_deleteMatrix( pFreqDomainSolution->numeratorNameAry
                           , /* noRows */ noDependents
                           , /* noCols */ noIndependents
                           );
        string_deleteMatrix( pFreqDomainSolution->denominatorNameAry
                           , /* noRows */ noDependents
                           , /* noCols */ noIndependents
                           );

        /* Delete the reference to the related parse result. */
        assert(pFreqDomainSolution->pTableOfVars != NULL);
        tbv_deleteTableOfVariables(pFreqDomainSolution->pTableOfVars);

        /* Delete the object's body. */
        free(pFreqDomainSolution);
    }

#ifdef DEBUG
    -- _noRefsToSolutionObjects;
#endif
} /* End of frq_deleteFreqDomainSolution */





/**
 * Get the name of an independent quantity, i.e. an input quantity all the solutions
 * offered by this object dependent on.
 *   @return
 * Get the name as a read-only string. The returned character pointer points into the
 * solution object and is valid as long as the solution object itself.
 *   @param pSolution
 * The pointer to the solution object.
 *   @param idxIndependent
 * The index of the independent, which is either 0 or identical to the index of a known in
 * the LES.
 */

const char * frq_getNameOfIndependent( const frq_freqDomainSolution_t * const pSolution
                                     , unsigned int idxIndependent
                                     )
{
    assert(pSolution != FRQ_NULL_SOLUTION);
    assert(idxIndependent < frq_getNoIndependents(pSolution));

    const char *name;
    const tbv_tableOfVariables_t * const pTableOfVars = pSolution->pTableOfVars;
    if(pSolution->idxResult >= 0)
    {
        /* Normal situation: This object is related to a user defined result. We need to
           make the distinction between a full result and a Bode plot. */
        assert((unsigned)pSolution->idxResult < pTableOfVars->pCircuitNetList->noResultDefs);
        const pci_resultDef_t * const pResultDef = &pTableOfVars
                                                    ->pCircuitNetList
                                                      ->resultDefAry[pSolution->idxResult];
        if(pResultDef->independentName == NULL)
        {
            /* A full result: The independents of this solution are the knowns of the LES. */
            assert(idxIndependent < pTableOfVars->noKnowns);
            name = pTableOfVars->knownLookUpAry[idxIndependent].name;
        }
        else
        {
            /* A Bode plot with one explicitly named dependent and independent. */
            assert(pResultDef->noDependents == 1);
            name = pResultDef->independentName;
        }
    }
    else
    {
        /* This object holds a full result for all unknowns of the LES. The independent is
           the known of the LES. */
        assert(idxIndependent < pTableOfVars->noKnowns);
        name = pTableOfVars->knownLookUpAry[idxIndependent].name;
    }

    return name;

} /* End of frq_getNameOfIndependent */




/**
 * Get the name of a dependent quantity, i.e. a quantity the solution object offers a
 * solution for.
 *   @return
 * Get the name as a read-only string. The returned character pointer points into the
 * solution object and is valid as long as the solution object itself.
 *   @param pSolution
 * The pointer to the solution object.
 *   @param idxDependent
 * The index of the dependent, which is identical to the index of the solutions held in
 * this object.
 */

const char * frq_getNameOfDependent( const frq_freqDomainSolution_t * const pSolution
                                   , unsigned int idxDependent
                                   )
{
    assert(pSolution != FRQ_NULL_SOLUTION);
    assert(idxDependent < frq_getNoDependents(pSolution));

    const char *name;
    const tbv_tableOfVariables_t * const pTableOfVars = pSolution->pTableOfVars;
    if(pSolution->idxResult >= 0)
    {
        /* Normal situation: This object is related to a user defined result with a sub-set
           of unknowns and/or user defined voltages as dependents. */
        assert((unsigned)pSolution->idxResult < pTableOfVars->pCircuitNetList->noResultDefs);
        const pci_resultDef_t * const pResultDef = &pTableOfVars
                                                    ->pCircuitNetList
                                                      ->resultDefAry[pSolution->idxResult];
        assert(idxDependent < pResultDef->noDependents);
        name = pResultDef->dependentNameAry[idxDependent];
    }
    else
    {
        /* This object holds a solution for all unknowns of the LES. The dependent is the
           unknown. */
        if(idxDependent < pTableOfVars->noUnknowns)
        {
            assert(idxDependent < pTableOfVars->noUnknowns);
            name = pTableOfVars->unknownLookUpAry[idxDependent].name;
        }
        else
        {
            /* The index refers to a user-defined voltage. These are addressed by index
               values immediately above the normal unknowns. */
            const pci_circuit_t * const pCircuitNetList = pTableOfVars->pCircuitNetList;
            unsigned int idxUserVoltage = idxDependent - pTableOfVars->noUnknowns;
            assert(idxUserVoltage < pCircuitNetList->noVoltageDefs);
            name = pCircuitNetList->voltageDefAry[idxUserVoltage].name;
        }
    }

    return name;

} /* End of frq_getNameOfDependent */




/**
 * Print the solution of the system in the frequency domain to the application log.
 *   @param pSolution
 * The pointer to the object representing the solution to be printed. The object is the result
 * of a successful call of frq_freqDomainSolution_t *frq_createFreqDomainSolution(const
 * sol_solution_t * const, unsigned int idxResult).
 *   @param hLog
 * The handle of a logger to be used for reporting.
 *   @param logLevel
 * The log level at which the output becomes visible. No output is created if the logger
 * object in use has a higher level.
 */

void frq_logFreqDomainSolution( const frq_freqDomainSolution_t * const pSolution
                              , log_hLogger_t hLog
                              , log_logLevel_t logLevel
                              )
{
    assert(pSolution != FRQ_NULL_SOLUTION);

    /* All logging is done at given level; we can take a shortcut if this verbosity is not
       desired. */
    if(!log_checkLogLevel(hLog, logLevel))
        return;

//    /* Log a title line, which also produces the line header. */
//    const char * const strIsBodePlot = frq_getIsBodePlot(pSolution)? " (Bode plot)": ""
//             , * const strFormat = pSolution->idxResult >= 0
//                                   ? "User-defined result %s%s:\n"
//                                   : "Result %s%s in the frequency domain:\n";
//    log_log(hLog, logLevel, strFormat, frq_getResultName(pSolution), strIsBodePlot);
    /* Log a line header only - printSolution will complete the line by printing the titel
       text. */
    log_log(hLog, logLevel, "");

    /* Logging is not done by the logger's methods but directly into the stdio streams.
       This enables us to implement the formatting functions to be used with any other
       streams also (e.g. for the Ocatve interface). */
    FILE *hStream[2];
    log_getStreams(hLog, &hStream[0], &hStream[1]);
    unsigned int idxStream;
    for(idxStream=0; idxStream<2; ++idxStream)
    {
        if(hStream[idxStream] != NULL)
        {
            if(!printSolution( pSolution
                             , hStream[idxStream]
                             , /* asOctaveCode */ false
                             , /* printMargin */ 72
                             )
              )
            {
                LOG_ERROR( _log
                         , "An arithmic overflow occured during rendering of the solution."
                           " The result representation is invalid and should be discarded"
                         )
            }
        }
    } /* End for(All possible open logging streams) */

    log_flush(hLog);

} /* End of frq_logFreqDomainSolution */



/**
 * Export the frequency domain solution as Octave M code.
 *   @return
 * Rendering the solution as an M function can fail because of arithmetic overflows. If no
 * problem appears then \a true is returned, \a false otherwise. The generated M code
 * should not be used if the function returns \a false.
 *   @param pSolution
 * The pointer to the object representing the solution to be printed. The object is the result
 * of a successful call of frq_freqDomainSolution_t *frq_createFreqDomainSolution(const
 * sol_solution_t * const, unsigned int idxResult).
 *   @param pMScript
 * The pointer to an M script object. The generated M code is written into this M script.
 * The object is the result of a successful call of boolean msc_createMScript(msc_mScript_t
 * ** const, const char * const, const char * const, const char * const).
 */

boolean frq_exportAsMCode( const frq_freqDomainSolution_t * const pSolution
                         , msc_mScript_t * const pMScript
                         )
{
    assert(pSolution != FRQ_NULL_SOLUTION);
    boolean success = true;
    const unsigned int noDependents = frq_getNoDependents(pSolution)
                     , noIndependents = frq_getNoIndependents(pSolution);
    assert(noDependents > 0  &&  noIndependents > 0);
    const tbv_tableOfVariables_t * const pTableOfVars = pSolution->pTableOfVars;

    /* The name of the solution is used as function name. */
    const char * const systemName = pSolution->name;

    /* First write the values of the needed device constants into the M script. This is
       done such that interaction with the embedding M code is possible without a change of
       the generated script. */
    /* Write into the stream associated with the M script object. */
    FILE *stream = msc_borrowStream(pMScript);
    print(stream, "error(nargchk(0, 1, nargin))\nif nargin == 1\n");
    msc_releaseStream(pMScript);
    tbv_exportAsMCode(pTableOfVars, pMScript, tbv_assignParameterStruct, "    ");
    stream = msc_borrowStream(pMScript);
    print(stream, "else\n");
    msc_releaseStream(pMScript);
    tbv_exportAsMCode(pTableOfVars, pMScript, tbv_assignDefaultValues,   "    ");
    stream = msc_borrowStream(pMScript);
    print(stream, "end\nerror(nargchk(0, 4, nargout))\nif nargout >= 2\n");
    msc_releaseStream(pMScript);
    tbv_exportAsMCode(pTableOfVars, pMScript, tbv_copyToParameterStruct, "    ");
    stream = msc_borrowStream(pMScript);
    print(stream, "end\n\n");

    if(!printSolution(pSolution, stream, /* asOctaveCode */ true, /* printMargin */ 72))
    {
        success = false;
        LOG_ERROR( _log
                 , "An arithmic overflow occured during rendering of the solution. The"
                   " result representation in the generated Octave script %s is invalid"
                   " and should be discarded"
                 , getFileName(pMScript)
                 )
        print(stream, "\nerror('Invalid script: Errors occured during script generation')\n");
    }

    /* Let Octave create the LTI system object. First we create a descriptor object. */
    print( stream
         , "\n%% Create an Octave LTI system object from the data above. First shape a"
           " descriptor object.\n"
         );
    print(stream, "systemDesc_%s.name = '%s';\n", systemName, systemName);

    /* Print the cell array of numerators like
         systemDesc_myResult.numeratorAry = ...
             { N_U_out_U1 N_U_out_U2
               N_U_K_L_U1 N_U_K_L_U2
             }; */
    print(stream, "systemDesc_%s.numeratorAry = ...\n    {", systemName);
    unsigned int idxDependent,  idxIndependent;
    for(idxDependent=0; idxDependent<noDependents; ++idxDependent)
    {
        for(idxIndependent=0; idxIndependent<noIndependents; ++idxIndependent)
            print(stream, " %s", getNameOfNumerator(pSolution, idxDependent, idxIndependent));

        /* A new line (for the next dependent) differs after writing the last dependent; we
           need to close the brace expression (i.e. the cell array). */
        if(idxDependent+1 < noDependents)
            print(stream, "\n     ");
        else
            print(stream, "\n    ");
    }
    print(stream, "};\n");

    /* Print the cell array of denominators like
         systemDesc_myResult.denominatorAry = ...
             { D_U_out_U1 D_U_out_U2
               D_U_K_L_U1 D_U_K_L_U2
             }; */
    print(stream, "systemDesc_%s.denominatorAry = ...\n    {", systemName);
    for(idxDependent=0; idxDependent<noDependents; ++idxDependent)
    {
        for(idxIndependent=0; idxIndependent<noIndependents; ++idxIndependent)
        {
            print( stream
                 , " %s"
                 , getNameOfDenominator(pSolution, idxDependent, idxIndependent)
                 );
        }

        /* A new line (for the next dependent) differs after writing the last dependent; we
           need to close the brace expression (i.e. the cell array). */
        if(idxDependent+1 < noDependents)
            print(stream, "\n     ");
        else
            print(stream, "\n    ");
    }
    print(stream, "};\n");

    /* Put the list of names of system inputs into the descriptor object. These are the
       independents. The ouput looks like:
         systemDesc_myResult.inputNameAry = ...
             { 'U1'
               'U2'
             }; */
    print(stream, "systemDesc_%s.inputNameAry = ...\n    {", systemName);
    for(idxIndependent=0; idxIndependent<noIndependents; ++idxIndependent)
    {
        print(stream, " '%s'", frq_getNameOfIndependent(pSolution, idxIndependent));

        /* A new line (for the next name) differs after writing the last name; we
           need to close the brace expression (i.e. the cell array). */
        if(idxIndependent+1 < noIndependents)
            print(stream, "\n     ");
        else
            print(stream, "\n    ");
    }
    print(stream, "};\n");

    /* Now we need a similar list of names for the system output, which are our dependents. */
    print(stream, "systemDesc_%s.outputNameAry = ...\n    {", systemName);
    for(idxDependent=0; idxDependent<noDependents; ++idxDependent)
    {
        print(stream, " '%s'", frq_getNameOfDependent(pSolution, idxDependent));

        /* A new line (for the next name) differs after writing the last name; we
           need to close the brace expression (i.e. the cell array). */
        if(idxDependent+1 < noDependents)
            print(stream, "\n     ");
        else
            print(stream, "\n    ");
    }
    print(stream, "};\n");

    /* Render the associated plot information object as Octave script code. */
    const pci_plotInfo_t *pPlotInfo;
    if(pSolution->idxResult >= 0)
    {
        const pci_circuit_t * const pCircuitNetList = pTableOfVars->pCircuitNetList;
        assert((unsigned)pSolution->idxResult < pCircuitNetList->noResultDefs);
        pPlotInfo = pCircuitNetList->resultDefAry[pSolution->idxResult].pPlotInfo;

        /* pPlotInfo is NULL if no plot information has been specified for the given
           result. */
    }
    else
        pPlotInfo = NULL;
    print(stream, "systemDesc_%s.plotInfo = ...\n", systemName);
    msc_releaseStream(pMScript);
    pci_exportPlotInfoAsMCode(pMScript, pPlotInfo, /* indentStr */ "    ");
    stream = msc_borrowStream(pMScript);
    print(stream, ";\n\n");

#if 0 // No longer used for an M code function file.

    /* Remove the no longer used expressions from the (global) workspace, like
         clear N_U_out_U1 N_U_out_U2 D_U_out_U1 D_U_out_U2 */
    print( stream
         , "%% Delete no longer used temporary expressions. They should not spoil"
           " the global workspace.\n"
           "clear"
         );
    unsigned int noPairsSoFar = 0;
    for(idxDependent=0; idxDependent<noDependents; ++idxDependent)
    {
        for(idxIndependent=0; idxIndependent<noIndependents; ++idxIndependent)
        {
            ++ noPairsSoFar;
            print( stream
                 , " %s %s"
                 , getNameOfNumerator(pSolution, idxDependent, idxIndependent)
                 , getNameOfDenominator(pSolution, idxDependent, idxIndependent)
                 );
        }

        /* Break line after some output. */
        if(noPairsSoFar % 2 == 0  &&  noPairsSoFar < noDependents*noIndependents)
            print(stream, " ...\n     "); /* strlen("clear") */
    }
    print(stream, "\n\n");
#endif

    /* Make Octave create the actual LTI system object by calling a creator function with
       the descriptor object shaped just before. */
    print(stream, "%% Create the Octave LTI system object from the descriptor.\n");
    print(stream, "tf_%s = createLtiSystem(systemDesc_%s);\n\n", systemName, systemName);

    /* Offer to produce a returned vector of frequency and time points. These series are
       made according to the plotInfo object in the circuit file. They can be passed to the
       member function of the Octave's transfer function class, like bode or step. */
    print( stream
         , "%% Compute a suitable vector of frequency and time points.\n"
           "wBode  = getFrequencyVector(systemDesc_%s);\n"
           "tiStep = getSampleTimeVector(systemDesc_%s);\n\n"
         , systemName
         , systemName
         );

    /* Let the script execute the most common operation with the LTI object. This is done
       in an if: If the function arguments are consumed by the (user owned and designed)
       embedding code then the function keeps silent and and only returns the transfer
       function object and the realted things for further, whatever use. */
    print(stream, "if nargout == 0\n");
    if(frq_getIsBodePlot(pSolution))
    {
        print(stream, "    %% Plot the transfer function of %s.\n", systemName);
        print( stream
             , "    figure\n"
               "    bode(tf_%s, wBode)\n"
             , systemName
             );
    }
    else
    {
        /* Octave refuses to print a Bode plot for MIMO systems. We print the step response
           as initial plot. */
        print(stream, "    %% Plot the step response of %s.\n", systemName);
        print( stream
             , "    figure\n"
               "    step(tf_%s, tiStep)\n"
             , systemName
             );
    }

    /* Let the script give some feedback to the user what happened and what he can do now. */
#define NL  " char(10) ...\n"
#define IND "          "
    print( stream
         , "    disp(["
           "'This function can create the LTI system object tf_%s for you; please, type'" NL
           IND "'help %s for more.'" NL
           IND "'  You can use this object with functions like bode to plot the transfer function'" NL
           IND "'or step and impulse to plot the step or impulse response time functions or lsim'" NL
           IND "'to compute or plot the system response to arbitrary input signals. A stability'" NL
           IND "'analysis can be done using function nyquist. Please refer to the online help'" NL
           IND "'for these commands' char(10)] ...\n" 
           "        );\n"
         , systemName
         , systemName
         );
#undef NL
#undef IND

    /* We are still in the if clause: no function results are consumed. Better to clear all
       output arguments; otherwise the user is enforced to use the line-closing semicolon
       when calling the function. */
    print( stream
         , "    clear tf_%s tiStep wBode\n"
           "end\n"
         , systemName
         );
    fflush(stream);

    msc_releaseStream(pMScript);

    return success;

} /* End of frq_exportAsMCode */




