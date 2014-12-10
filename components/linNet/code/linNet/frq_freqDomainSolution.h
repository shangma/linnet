#ifndef FRQ_FREQDOMAINSOLUTION_INCLUDED
#define FRQ_FREQDOMAINSOLUTION_INCLUDED
/**
 * @file frq_freqDomainSolution.h
 * Definition of global interface of module frq_freqDomainSolution.c
 *
 * Copyright (C) 2013 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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

/*
 * Include files
 */

#include "log_logger.h"
#include "sol_solver.h"
#include "rat_rationalNumber.h"
#include "tbv_tableOfVariables.h"
#include "msc_mScript.h"


/*
 * Defines
 */

/** Indication, that this is not a valid object. Used instead of the pointer to a true
    object. */
#define FRQ_NULL_SOLUTION   NULL


/*
 * Global type definitions
 */

/** One addend of a coefficient of an expression in the frequency domain. */
typedef struct frq_frqDomExpressionAddend_t
{
    /** An expression in the frequency domain is a linked list of these addends. The link
        to the next addend is made by this pointer. NULL for the last or a single addend.
          @remark It's essential to have this member as the very first one. Its address
        needs to be identical to the address of the entire struct. */ 
    struct frq_frqDomExpressionAddend_t *pNext;
    
    /** The numeric factor of the product of powers of constants. */
    rat_num_t factor;
    
    /** A product of powers of constants. Array element n is related to a particular
        constant. This constant is taken to the power \a powerOfConstAry[n]. */
    signed int powerOfConstAry[COE_MAX_NO_CONST];
    
    /** The addend is the product of numeric factor and constants to the given power and
        the frequency variable s to a given power. The power of s is found here. */
    signed int powerOfS;
    
} frq_frqDomExpressionAddend_t;


/** A complete expression in the frequency domain. Actually identical to one of its addends
    as an expression is a linked list of such addends. The other type name is mainly used
    for code readability: The heading addend represents the complete expression at the
    same time.
      @remark This is a denormalized expression, as opposed to the normalized expression,
    see \a frq_normalizedFrqDomExpression_t. */
typedef frq_frqDomExpressionAddend_t frq_frqDomExpression_t;


/** A normalized expression in the frequency domain. It has a list of addends, the actual
    expression and a common factor, also an addend, which is used for normalization.\n
      Different to an ordinary, denormalized expression, see \a frq_frqDomExpression_t, the
    "actual expression" has a defined range of powers. The individual powers of s and of the
    device constants are all positive and they are guaranteed to begin with the lowest
    power null. */
typedef struct frqx_frqDomExpression_t
{
    /** The common factor. All terms of * \a pExpr have to be multiplied with this factor. */
    frq_frqDomExpressionAddend_t *pFactor;
    
    /** The remaining terms of the expression. Actually a list of addends. */
    frq_frqDomExpression_t *pExpr;

} frq_normalizedFrqDomExpression_t;



/** A matrix A[m*n] of normalized frequency domain expressions. It is defined as array of
    pointers to rows. This permits dynamic allocation of matrixes of any size. */
typedef frq_normalizedFrqDomExpression_t ***frq_normalizedFrqDomExpressionMatrix_t;


/** The solution object contains the solution of the LES in the frequency domain. */
typedef struct frq_freqDomainSolution_t
{
    /** A counter of references to this object. Used to control deletion of object. */
    unsigned int noReferencesToThis;
    
    /** The name of this solution. The name corresponds with the name of the user defined
        result this solution object belongs to. */
    const char *name;

    /** The table of variables describes the knowns, unknowns and constants of the problem;
        among more, their names are found here. */
    const tbv_tableOfVariables_t *pTableOfVars;

    /** The index of the user-demanded result, which this solution object represents. The
        index is related to the table of result definitions in the \a pci_circuit_t object
        inside of * \a pTableOfVars.\n
          A value of -1 means that this object holds a solution for all the unknowns of the
        LES regardless of user requested sub-sets of variables. */
    signed int idxResult;
    
    /** The system determinant and the common denominator of all result terms. */
    frq_normalizedFrqDomExpression_t *pDenominator;

    /** An array [noSolutions, noKnowns] of numerators of result terms. There's one such
        term for each known in each solution. */
    frq_normalizedFrqDomExpressionMatrix_t numeratorAry;

    /** The names of all numerator expressions, used e.g. as variable names in the
        generated Octave script. */
    const char * **numeratorNameAry;
    
    /** The names of all (final) denominator expressions, used e.g. as variable names in the
        generated Octave script. */
    const char * **denominatorNameAry;

} frq_freqDomainSolution_t;


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */

/** Initialize the module prior to first use of any of its methods or global data objects. */
void frq_initModule(log_hLogger_t hGlobalLogger);

/** Shutdown of module after use. Release of memory, closing files, etc. */
void frq_shutdownModule(void);

/** Create a result representation in the frequency domain. */
boolean frq_createFreqDomainSolution( const frq_freqDomainSolution_t * * const ppFrqDomSolution
                                    , const sol_solution_t * const pAlgebraicSolution
                                    , signed int idxResult
                                    );

/** Get another reference to the same object. */
frq_freqDomainSolution_t *frq_cloneByReference(frq_freqDomainSolution_t * const pExistingObj);

/** Get another read-only reference to the same object. */
const frq_freqDomainSolution_t *frq_cloneByConstReference
                                        (const frq_freqDomainSolution_t * const pExistingObj);
                                        
/** Delete a result representation in the frequency domain or a reference to it after use. */
void frq_deleteFreqDomainSolution(const frq_freqDomainSolution_t * const pFreqDomainSolution);

/** Get the name of an independent quantity or a given input quantity. See the public
    inline interface for the number of such. */
const char * frq_getNameOfIndependent( const frq_freqDomainSolution_t * const pSolution
                                     , unsigned int idxIndependent
                                     );

/** Get the name of a dependent quantity or a resulting quantity. See the public inline
    interface for the number of such. */
const char * frq_getNameOfDependent( const frq_freqDomainSolution_t * const pSolution
                                   , unsigned int idxSolution
                                   );

/** Print a complete solution in the frequency domain to the application log. */
void frq_logFreqDomainSolution( const frq_freqDomainSolution_t * const pSolution
                              , log_hLogger_t hLog
                              , log_logLevel_t logLevel
                              );

/** Export a complete solution in the frequency domain as Octave script code. */
boolean frq_exportAsMCode( const frq_freqDomainSolution_t * const pSolution
                         , msc_mScript_t * const pMScript
                         );

#endif  /* FRQ_FREQDOMAINSOLUTION_INCLUDED */
