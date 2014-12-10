/**
 * @file sol_solver.c
 * Symbolic solver for linear equations systems (LES)
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
 *   sol_initModule
 *   sol_shutdownModule
 *   sol_createSolution
 *   sol_cloneByReference
 *   sol_cloneByConstReference
 *   sol_deleteSolution
 *   sol_getNoIndependents
 *   sol_getNameOfIndependent
 *   sol_getNoDependents
 *   sol_findName
 *   sol_getNameOfDependent
 *   sol_logSolution
 * Local functions
 *   getVectorOfReqDependents
 *   elementaryStep
 *   solverLES
 */


/*
 * Include files
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "smalloc.h"
#include "log_logger.h"
#include "coe_coefficient.h"
#include "coe_coefficient.inlineInterface.h"
#include "les_linearEquationSystem.h"
#include "sol_solver.h"


/*
 * Defines
 */


/*
 * Local type definitions
 */


/*
 * Local prototypes
 */


/*
 * Data definitions
 */

/** A global logger object is referenced from anywhere for writing progress messages. */
static log_hLogger_t _log = LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT;

#ifdef  DEBUG
/** A global counter of all references to any created objects. Used to detect memory leaks. */
static unsigned int _noRefsToObjects = 0;
#endif


/*
 * Function implementation
 */


/**
 * The set of dependents(i.e. the unknowns of the LES and the user defined voltages), which
 * are required to compute all user defined results is figured out. All unknowns of the
 * LES, which are not in the set don't need to be computed by the solver. The set considers
 * all user-defined results, which may directly refer to unknowns of the LES but also
 * indirectly by referencing user-defined voltages.
 *   @return
 * The set is returned as an ordered vector of Boolean, where element \a i of the vector
 * relates to unknown \a i of the LES, if \a i is lower then the number \a n of unknowns.
 * Otherwise element \a i refers to user defined voltage \a i - \a n. The related
 * dependent is required if the element is \a true.\n
 *   The returned vector is malloc allocated and needs to be freed after use.
 *   @param pSolution
 * The pointer to the solution object. If called during object construction: The table of
 * variables and the circuit net list need to be setup before this function may be called.
 */

static const boolean *getVectorOfReqDependents(const sol_solution_t * const pSolution)
{
    const tbv_tableOfVariables_t * const pTableOfVars = pSolution->pTableOfVars;
    const pci_circuit_t * const pCircuitNetList = pTableOfVars->pCircuitNetList;
    
    /* The result of this function is a Boolean vector, one element for each computable
       dependent, either a true unknown of the LES or a derived dependent, i.e. a
       user-defined voltage. */
    const unsigned int noUnknowns  = pTableOfVars->noUnknowns 
                     , noVoltagesDefs = pCircuitNetList->noVoltageDefs
                     , noDependents = noUnknowns+noVoltagesDefs;
    boolean * const pVectorOfReqDependents = smalloc( sizeof(*pVectorOfReqDependents)
                                                      * noDependents
                                                    , __FILE__
                                                    , __LINE__
                                                    );
    
    /* The list of required unknowns is solely determined by the user defined results.
       Iterate over all such results. If there's no user-defined result: All unknowns are
       requested by default. */
    const unsigned int noUserResults = pCircuitNetList->noResultDefs;
    unsigned int u;
    for(u=0; u<noDependents; ++u)
        pVectorOfReqDependents[u] = noUserResults == 0;
    if(noUserResults > 0)
    {
        unsigned int u;
        for(u=0; u<noUserResults; ++u)
        {
            const pci_resultDef_t * const pRes = &pCircuitNetList->resultDefAry[u];
            
            /* Look for all variables, which are referenced from the result. Due to the
               fact that a result can also be an inverse transfer function, we can't say a
               priori whether the in- or the output variable of a result is a dependent of
               the LES - we have to consider them both. */
            
            /* Create an array with the names of all referenced variables, regardless
               whether it are dependents or independents to the result. The array of
               dependents of the result can be copied, the only independent to the result,
               which could actually be a dependent to the LES is appended in case. */
            unsigned int noResVars = pRes->noDependents;
            const char *nameOfResVars[noResVars+1];
            memcpy( nameOfResVars
                  , pRes->dependentNameAry
                  , sizeof(nameOfResVars[0]) * noResVars
                  );
            if(pRes->independentName != NULL)
                nameOfResVars[noResVars++] = pRes->independentName;

            /* Now check all the names but ignore them if they are found to be independents
               of the LES. */
            unsigned int idxResVar;
            for(idxResVar=0; idxResVar<noResVars; ++idxResVar)
            {
                /* A user defined result references the unknowns of the LES (and the
                   user-defined voltages) by name. These references might be bad, the input
                   has not yet been validated. We need to anticipate invalid or ambiguous
                   names. Ambiguities are reported by the function result, the returned
                   index tells if it is a dependent with respect to the LES. */
                signed int idxDependent;   
                const unsigned int noMatches = sol_findName( pSolution
                                                           , &idxDependent
                                                           , /* pIdxUnknown       */ NULL
                                                           , /* pIdxUserDefVoltage*/ NULL
                                                           , /* pIdxKnown         */ NULL
                                                           , nameOfResVars[idxResVar]
                                                           , /* doErrorReporting */ false
                                                           );
                if(noMatches == 1  &&  idxDependent >= 0)
                {
                    /* The required dependent can be both, a true unknown of the LES or a
                       user-defined voltage. The latter still need to be resolved to true,
                       computed unknowns. */
                    assert((unsigned)idxDependent < noDependents);
                    pVectorOfReqDependents[idxDependent] = true;
                    LOG_DEBUG( _log
                             , "Dependent %s (%d) is required for user defined result %s"
                             , nameOfResVars[idxResVar]
                             , idxDependent
                             , pRes->name
                             );
                }
                else
                {
                    /* Error feedback is not given here; this is done later in module frq,
                       when presenting the results. */
                }
            } /* End of for(All variables referenced from result definition) */ 
            
        } /* End for(All result definitions) */
        
        /* Here, we know the list of required dependents with respect to this solution
           object, however a sub-set of these dependents may be user-defined voltages,
           which in turn depend on true unknowns of the LES. This has to be resolved as the
           calling code, the actual solver, needs to know, which unknowns to compute. */
    
        unsigned int idxVolDef;
        for(idxVolDef=0; idxVolDef<noVoltagesDefs; ++idxVolDef)
        {
            assert(noUnknowns+idxVolDef < noDependents);
            if(pVectorOfReqDependents[noUnknowns+idxVolDef])
            {
                const pci_voltageDef_t * const pVoltageDef =
                                                &pCircuitNetList->voltageDefAry[idxVolDef];
                                                
                /* Find the index of the unknowns, which represent those nodes' voltages,
                   which are defined to be plus and minus pole of the user defined voltage.
                   Both of them can be the ground node, in which case no unknown is
                   required. This is indicated by index UINT_MAX. */
                const tbv_unknownVariable_t *pUnknown =
                                tbv_getUnknownByNode( pTableOfVars
                                                    , /* idxNode */ pVoltageDef->idxNodePlus
                                                    );
                if(pUnknown != NULL)
                {
                    assert(pUnknown->idxCol < noUnknowns);
                    pVectorOfReqDependents[pUnknown->idxCol] = true;
                    LOG_DEBUG( _log
                             , "Dependent %s (%d) is required for user defined voltage %s"
                             , pUnknown->name
                             , pUnknown->idxCol
                             , pVoltageDef->name
                             );
                }
                
                pUnknown = tbv_getUnknownByNode( pTableOfVars
                                               , /* idxNode */ pVoltageDef->idxNodeMinus
                                               );
                if(pUnknown != NULL)
                {
                    assert(pUnknown->idxCol < noUnknowns);
                    pVectorOfReqDependents[pUnknown->idxCol] = true;
                    LOG_DEBUG( _log
                             , "Dependent %s (%d) is required for user defined voltage %s"
                             , pUnknown->name
                             , pUnknown->idxCol
                             , pVoltageDef->name
                             );
                }

            } /* End if(Is this a required user defined voltage?) */
            
        } /* End for(All user defined voltages) */
        
    } /* End if(Do we have user-defined results or should all dependents be computed?) */

    return pVectorOfReqDependents;

} /* End of getVectorOfReqDependents */




/**
 * The most basic operation of the extended Gauss elimination. It re-computes one
 * coefficient of the LES according to: A(m,n)=(A(m,n)*A(step,step)-A(step,n)*A(m,step))/t.
 * The extension of the known Gauss elimination is the specification of t, which is a known
 * divisor; the division can be carried out without a remainder. Normally the Gauss
 * elimination is defined for algebraic structure of kind field; because of the divisor we
 * can apply it to the simpler ring.\n
 *   Here, the elementary step is implemented for linear combinations having products of
 * symbols as coefficients.
 *   @param A
 * The matrix of coefficients, which are manipulated in place. The result is returned in
 * place.
 *   @param step
 * The elimination step of the Gauss elimination. In fact, step+i, i = 1..m-step-1, are the
 * indexes of the equations which are manipulated to get another leading null coefficient.
 *   @param row
 * The row index of the coefficient under operation.
 *   @param col
 * The column index of the coefficient under operation.
 *   @param pKnownDivisor
 * The known divisor.
 */

static void elementaryStep( coe_coefMatrix_t A
                          , signed int step
                          , signed int row
                          , signed int col
                          , const coe_coef_t * const pKnownDivisor
                          )
{
    /* If the current diagonal element is null then the determinant is already known to be
       null and we must not have got here. */
    assert(!coe_isCoefAddendNull(A[step][step]));

    /* In the first step we compute the numerator of the quotient as the sum of two
       products of two coefficients each. At the end of this step *pNumerator will take the
       value A(m,n)*A(step,step)-A(step,n)*A(m,step). (See below for a refinement of this
       statement.) We start with null and add two times the products of all combinations of
       terms from first and second coeffcient. */
    coe_coef_t *pNumerator = coe_coefAddendNull();
    const coe_productOfConst_t prodOfCDiv = pKnownDivisor->productOfConst;

    /* The outer loop implements the sum of the two products with different sign. */
    signed int sign;
    const coe_coef_t *pOperand2;
    const coe_coefAddend_t *pAddend1;
    for( sign = +1, pAddend1 = A[row][col], pOperand2 = A[step][step]
       ; sign >= -1
       ; sign -= 2, pAddend1 = A[step][col], pOperand2 = A[row][step]
       )
    {
        /* Loop over all addends of first operand. */
        while(!coe_isCoefAddendNull(pAddend1))
        {
            assert(pAddend1->factor == 1  ||  pAddend1->factor == -1);
            const coe_productOfConst_t prodOfC1 = pAddend1->productOfConst;

            /* Each addend of the second operand is combined with the current addend of the
               first operand. Re-initialize the pointer to the addends of the second
               coefficient. */
            const coe_coefAddend_t *pAddend2 = pOperand2;
            while(!coe_isCoefAddendNull(pAddend2))
            {
                assert(pAddend1->factor == 1  ||  pAddend1->factor == -1);
                const coe_productOfConst_t prodOfC2 = pAddend2->productOfConst;

                /* It is proven that the numerator of the quotient will eventually contain
                   only such addends, which can be divided by the first addend of the
                   denominator such that all resulting constants have powers of null or one.
                   Intermediate results could basically occur here that do not fit into this
                   pattern but they would sooner or later be anyway eliminated by similar
                   addends of inverse sign. Therefore we may decide to immediately discard
                   those addends.
                     The first part of the condition discards addends, which can't be
                   devided by the first addend of the denominator (they don't contain the
                   combination of constants of the denominator term).
                     The second part of the condition discards addends, which would lead to
                   powers of constants greater than one in the result. */
                if(((~prodOfC1 & ~prodOfC2 & prodOfCDiv)
                    | (prodOfC1 & prodOfC2 & ~prodOfCDiv)
                   ) == 0
                  )
                {
                    /* This term is relevant for the final result. */

                    /* The product is computed and added to the numerator of the quotient.
                       However, not as such but already divided by the first addend of the
                       divisor. Just keep in mind that the first step of the later division
                       has already been done. (If we'd truely store the product, we'd need
                       another representation of the constants: The products can have
                       constants to the power of two.)
                         The exclusive or adds the bits (i.e. powers of constants) but
                       because of the overrun it also implements the subtraction of the
                       divisor powers. The result is correct as we have already checked
                       that the final result for each bit is in the implemented range of
                       0..1.
                         No overflow recognition has been implemented for the numeric
                       factor. The result of the computation is the sum of two products of
                       two coefficients each. The combined four coefficients are either the
                       original coefficients of the LES or the result of the solver in the
                       previous elimination step. The numeric factor of all addends of all
                       of these four coefficients have the absolute value of one.
                       Consequently, each addend of the intermediate result (which is a
                       product of two addends from the coefficients) has a numeric factor
                       of absolute one, too. Factors not equal to absolute one can appear
                       in the intermediate result but they are yielded only by
                       incrementally adding another addend with a factor of (absolute) one,
                       so the factor in the intermediate result can increase only in steps
                       of one with each computed addend of the resulting sum of products.
                       There's no faster accumulation of the factor e.g. by multiplicative
                       effects. An overrun would therefore occur at earliest after having
                       figured out 2^31 products of addends. (And in fact it would require
                       many times more products as most of the terms are of course not
                       identical with respect to the product of constants.) An overrun can
                       thus happen only after a pseudo-infinite computation time and a
                       recognition of such an overrun would not help in any fashion as
                       nobody would ever wait for this response of the software.
                         Practically spoken, if the algorithm ends in finite time then
                       surely no overrun occurred. */
                    coe_addAddend( &pNumerator
                                 , /* factor */ pAddend1->factor * (sign > 0
                                                                    ? pAddend2->factor
                                                                    : -pAddend2->factor
                                                                   )
                                 , /* productOfConsts */ prodOfC1 ^ prodOfC2  ^ prodOfCDiv
                                 );
                } /* End if(Is this term relevant for the final result?) */

                pAddend2 = pAddend2->pNext;

            }/* Ende der Schleife ueber die Liste des 2. Operanden */

            pAddend1 = pAddend1->pNext;

        } /* End while(All addends of the first operand of the product) */

    } /* End for(Both products of the numerator) */

    /* At this point *pNumerator holds all terms of the numerator of the quotient, which
       will contribute to the final result. All of them are divisible by the first addend
       of the divisor and actually, we have already divided them by the product of
       constants of this addend.
         In the second part of the elementary operation the division is done by the
       algorithm of the polynomial division. */
    coe_coef_t *pResult = coe_coefAddendNull()
             , **ppResultEnd = &pResult;

    const coe_numericFactor_t factorDiv = pKnownDivisor->factor;

    /* The factor of all addends that are input to the elementary step is always absolute
       one. As opposed to this, the numeric factors of the addends of *pNumerator may have
       values other than absolute one. */
    assert(factorDiv == 1  ||  factorDiv == -1);

    /* We loop over all addends of the numerator. */
    while(!coe_isCoefAddendNull(pNumerator))
    {
        /* Here we test the numeric factor of an addend from the internal numerator
           variable. The numeric factors of the numerator variable are not per se absolute
           one; only here and now, when the addend becomes the addend under progress the
           factor needs to be one. If not, the resulting addend of the final result would
           have a numeric factor other than one and this is proven to be impossible. */
        assert(pNumerator->factor == 1  ||  pNumerator->factor == -1);

        /* It is proven, that the numeric quotient of the factors can be computed without a
           remainder. */
        assert(pNumerator->factor % factorDiv == 0);
        const coe_numericFactor_t factorRes = pNumerator->factor / factorDiv;

        /* The division of products of constants has already been conducted. Just hold a
           faster reference to the result. */
        const coe_productOfConst_t prodOfCRes = pNumerator->productOfConst;

        /* Put the new result term into the result coefficient. The terms are sorted in the
           order of falling binary interpretation of the product of constants and the
           result terms appear in exactly this order. So we can be sure, that the added
           term always has to be appended to the end of the list of addends. */
        coe_coefAddend_t *pNewResultAddend = coe_newCoefAddend();
        pNewResultAddend->factor = factorRes;
        pNewResultAddend->productOfConst = prodOfCRes;
        *ppResultEnd = pNewResultAddend;
        ppResultEnd = &pNewResultAddend->pNext;

        /* The new addend of the result is now multiplied with all the terms of the divisor
           and all terms of this product are subtracted from the numerator. Evidently, the
           first subtracted term will clear the addend in progress from the numerator. Less
           evident and due to the sort order of the addends in all the coefficients, all
           further terms will be located to left of this first term or with other words,
           the operation will discard the head of the numerator list and insert a number of
           new addends somewhere behind. */
        const coe_coefAddend_t *pAddendDiv = pKnownDivisor;
        do
        {
            assert(pAddendDiv->factor == -1  ||  pAddendDiv->factor == 1);
            const coe_productOfConst_t prodOfCAddendDiv = pAddendDiv->productOfConst;

            /* Here we have the complement to the computation of the numerator. There we'd
               discarded terms that would anyway not remain in the computation. Here is the
               second source for such terms. At latest the terms produced here would
               eliminate the irrelevant terms of the numerator computation. Since we had
               discarded those it is a must to discard these as well. */
            if(((~prodOfCAddendDiv & ~prodOfCRes & prodOfCDiv)
                | (prodOfCAddendDiv & prodOfCRes & ~prodOfCDiv)
               ) == 0
              )
            {
                /* The prove of the finiteness of the algorithm is based on the sort order
                   of terms in the coeffcients. No term, which is again inserted in the
                   numerator must have a combination of constants that is greater than that
                   of the term under progress (where "greater" refers to a comparison, which
                   interprets the product of constants as an unsigned binary number). */
                assert(pAddendDiv == pKnownDivisor
                       || (prodOfCAddendDiv ^ prodOfCRes ^ prodOfCDiv) < prodOfCRes
                      );

                /* This term is relevant for the final result.
                     No overrun recognition is required here, for the same reason as
                   documented for the call of coe_addAddend above. */
                coe_addAddend( &pNumerator
                             , /* factor */ - pAddendDiv->factor * factorRes
                             , /* productOfConsts */ prodOfCAddendDiv ^ prodOfCRes ^ prodOfCDiv
                             );
            }

            /* Handle product of result term with next addend of divisor. */
            pAddendDiv = pAddendDiv->pNext;
        }
        while(!coe_isCoefAddendNull(pAddendDiv));

    } /* End while(Divide all addends of the numerator) */

    /* Terminate the result list of addends. Eventually we need to write the NULL pointer. */
    *ppResultEnd = NULL;

    /* The numerator should be entirely processed. */
    assert(coe_isCoefAddendNull(pNumerator));

    /* Put the result directly into the matrix. Do a replace by first freeing the current
       element. */
    coe_freeCoef(A[row][col]);
    assert(coe_checkOrderOfAddends(pResult));
    A[row][col] = pResult;

} /* End of elementaryStep */






/**
 * The solver for a linear equation system.\n
 *   The system is solved by the modified Gaussian elimination method. The required
 * algebraic structure the coefficients belongs to is less demanding than for the original
 * elimination: The division is not required. The modified elimination can e.g. be
 * performed with integer numbers. The coefficients' algebraic structure needs to have the
 * sum and the multiplication operation.\n
 *   The arithmetic is not done in this routine; it only implements the control structure.
 * The algebraic operations on the coefficients are solely done in the sub-routine
 * elementaryOperation. By exchanging this sub-routine the solver could be used for other
 * sets of coefficients.\n
 *   A possible impact of the actual arithmetics on the control structure in this routine
 * could be a round off error of the multiplikation operation. True pivoting could
 * become an issue. The current implementation of the control structure just implements a
 * simple pivoting, which suffices for error-free sum and multiplication.
 *   @return
 * The function returns true if the LES could be solved. false ii e.g. returned in case of
 * linearly dependent equations.
 *   @param A
 * The array of coefficients, which are manipulated in place. The result is returned in
 * place.\n
 *   The array is organized as m rows and n columns, where n >= m+1. The rectangular area
 * A[0..m-1][0..m-1] holds the coefficients belonging to the m unknowns. If the algorithm
 * returns true than this area is a diagonal matrix, where A[i][i], i=0..m-1, holds the
 * denominator of the solution for unknown i.\n
 *   A[0..m-1][m..n-1] holds the left hand side of the LES (where the sign of these
 * coefficients is such that the sum of all row elements becomes null). Column i, i=m..n-1,
 * holds the coefficients belonging to the known input variable i-m. If the algorithm
 * returns true than this area contains the numerators of the solution. A[i][j+m]*V_j,
 * i=0..m-1, j=0..n-m-1, is the complete numerator of the result for unknown i.
 *   @param m
 * The number \a m of rows of the matrix \a A.
 *   @param n
 * The number \a n of columns of the matrix \a A.
 *   @see void elementaryStep(coe_coefMatrix_t, int, int, int, coe_coef_t *)
 */

static boolean solverLES( coe_coefMatrix_t A
                        , const unsigned int m
                        , const unsigned int n
                        )
{
    /* The first elimination step doesn't know a common divisor yet. The divisor is
       initialized with one. */
    coe_coefAddend_t addendOne = {.pNext = NULL, .factor = 1, .productOfConst = 0};
    coe_coef_t *pDivisor = &addendOne;

    /* Do all m-1 elimination steps. */
    unsigned int elimStep;
    boolean doSignInversion = false;
    for(elimStep=0; elimStep<m-1; ++elimStep)
    {
        /* Pivoting: Avoid a generalizing product with a null coefficient, which would
           break the algorithm. We look for a line, whose coefficient under progress is not
           null. Such a line needs to exist, otherwise it was the prove that the LES
           contains linear dependent or contradictory equations. */
        if(coe_isCoefAddendNull(A[elimStep][elimStep]))
        {
            unsigned int idxPivotRow = elimStep;
            do
            {
                if(++idxPivotRow >= m)
                {
                    /* No usable line found at all. The network analysis, which is part of
                       making the LES, failed to recognize the problem when checking the
                       (physical) constraints, which apply to an electric circuit. */
                    LOG_ERROR( _log
                             , "Gauss elimination of LES is aborted. Pivoting doesn't find"
                               " any non null coefficient in the %u. elimination step. The"
                               " equations are linear dependent or contradictory. Please"
                               " double-check your circuit net list"
                             , elimStep+1
                             )
                    return false;
                }
            }
            while(coe_isCoefAddendNull(A[idxPivotRow][elimStep]));

            LOG_DEBUG( _log
                     , "Pivoting in elimination step %u: Line exchange %u <-> %u"
                     , elimStep
                     , elimStep, idxPivotRow
                     );

            /* Exchange the row of the current elimination step with the pivot row. */
            coe_coef_t **elimRow = A[elimStep];
            A[elimStep] = A[idxPivotRow];
            A[idxPivotRow] = elimRow;

            /* Each exchange of rows means a sign change of the determinant of the LES. We
               keep track of this since we want to have the final solution in a form, where
               the denominator is this determinant. */
            doSignInversion = !doSignInversion;

        } /* End if(Pivot element is null) */

        /* Do the coefficient elimination/manipulation for all remaining, not yet handled
           rows and columns. */
        unsigned int row;
        for(row=elimStep+1; row<m; ++row)
        {
            unsigned int col;
            for(col=elimStep+1; col<n; ++col)
            {
                /* Elimination at A(row,col) */
                elementaryStep(A, elimStep, row, col, pDivisor);
            }

            /* We set the eliminated coefficient explicitly to null. This operation is
               useless with respect to the wanted result but it frees some memory and is
               advantageous for logging purpose. */
            coe_freeCoef(A[row][elimStep]);
            A[row][elimStep] = coe_coefAddendNull();
        }

        /* Remind the divisor of the next elimination step. */
        pDivisor = A[elimStep][elimStep];

#if 0
        /* If memory usage is an important matter, then we can free the coefficients of the
           upper lines of the matrix. We will end up with only the n-m+1 coefficients of
           the last line, that represent the result plus the divisor of the last
           elimination step. For logging purpose it might however be better not do so. */

        /* Loop over the line above those, where we eliminated the heading column. */
        unsigned int col;
        for(col=0; col<n; col++)
        {
            /* The elements on the diagonal are the still used divisors of the next
               elimination step. Don't free the last recent one. */
            if(col != elimStep)
                coe_freeCoef(A[elimStep][col]);
        }
        /* The no longer used divisor of the previous elimination step can now be freed. */
        if(elimStep > 0)
            coe_freeCoef(A[elimStep-1][elimStep-1]);
#endif
    } /* End for(All m-1 elimination steps) */

    /* The result is now immediately available for the last unknown in the last row. The
       denominator of this result is the system determinant if we consider possible
       intermediate sign changes caused by pivoting. We want to do so. */
    if(doSignInversion)
    {
        /* All non null coefficients of the last equation after the last elimination step
           are sign inverted. The equation is thus not changed. */
        unsigned int col;
        for(col=m-1; col<n; ++col)
            coe_mulConst(A[m-1][col], /* constant */ -1);
    }

    return true;

} /* End of solverLES */





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
 *   @see void sol_shutdownModule()
 */

void sol_initModule(log_hLogger_t hLogger)
{
    assert(hLogger != LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT);
    _log = log_cloneByReference(hLogger);

#ifdef  DEBUG
    /* The DEBUG compilation counts all references to all created objects. */
    _noRefsToObjects = 0;
#endif
} /* End of sol_initModule */




/**
 * Do all cleanup after use of the module, which is required to avoid memory leaks,
 * orphaned handles, etc.
 */

void sol_shutdownModule()
{
#ifdef  DEBUG
    /* The DEBUG compilation looks for still allocated objects in order to detect memory
       leaks. */
    if(_noRefsToObjects != 0)
    {
        fprintf( stderr
               , "sol_shutdownModule: %u references to objects of type"
                 " sol_solution_t have not been discarded at application"
                 " shutdown. There are probable memory leaks\n"
               , _noRefsToObjects
               );
    }
#endif

    /* Discard reference to the logger object. */
    log_deleteLogger(_log);

} /* End of sol_shutdownModule */




/**
 * The complete solution of a LES is figured out and returned as a new object.
 *   @return
 * true if and only if the operation entirely succeeded. Otherwise the function returns
 * false and appropriate error messages have been written to the global application log.
 *   @param ppSolution
 * The pointer to the new object is returned in * \a ppSolution. The solution object can be
 * used to report the result of the LES or for further processing (like numeric
 * evaluation). Do not use * \a ppSolution if the function returns false.
 *   @param pLES
 * The LES is passed by reference. The passed object is the successful result of
 * les_createLES. * \a pLES may be deleted after return of this function.
 *   @see void sol_deleteSolution(const sol_solution_t * const)
 */

boolean sol_createSolution( const sol_solution_t * * const ppSolution
                          , les_linearEquationSystem_t * const pLES
                          )
{
    sol_solution_t *pSol = smalloc(sizeof(sol_solution_t), __FILE__, __LINE__);

    /* The result of the constructor is the first reference to the new object. */
    pSol->noReferencesToThis = 1;
#ifdef DEBUG
    ++ _noRefsToObjects;
#endif

    /* Make a shallow copy of the table of variables. This copy is first used locally
       here and then passed on to the newly created object.
         We need a shallow copy to freeze the current set and order of the unknowns. The
       solver will require to resort the knowns in the course of the computation. This
       reordering will take place on the table of variables owned by the LES object. The
       solution object however requires a fixed order of unknowns as it uses the same order
       for storing the different terms of the LES' solution. */
    tbv_tableOfVariables_t * const pTabOfVars = tbv_cloneByShallowCopy(pLES->pTableOfVars);
    pSol->pTableOfVars = pTabOfVars;

    /* Retrieve the dimension of the LES. */
    unsigned int noKnowns, noUnknowns, noConstants;
    les_getNoVariables(pLES, &noKnowns, &noUnknowns, &noConstants);

    /* The number of knowns can easily be null if no constant source is in use. But there
       should always be at least one device, hence two nodes, hence one unknown. */
    assert(noUnknowns > 0);

    /* The user can define any number of voltages of interest as differences of node
       potentials. All of these will be considered in the result reporting. */
    const unsigned int noUserDefVoltages = pTabOfVars->pCircuitNetList->noVoltageDefs;

    /* There's one result for each unknown and for each voltage of interest (which can be
       identical to an unknown). Each such result has as many terms as there are
       knowns. The elements of the matrix are preset with null coefficients. */
    pSol->numeratorAry = coe_createMatrix(noUnknowns+noUserDefVoltages, noKnowns);
    pSol->pDeterminant = coe_coefAddendNull();

    /* Get the list of unknows, we really have to figure out. Normally, the user will
       request only a sub-set of all unknown quantities for plotting or printing. */
    pSol->pIsDependentAvailableAry = getVectorOfReqDependents(pSol);
    
    /* The core solver can bring the LES only in the shape of an upper triangular matrix,
       i.e. only the solution of a single unknown is entirely figured out. This unknown is
       the last one in the order of columns. The function here overcomes this limitation by
       running the core solver once per unknown. Prior to calling the core solver it
       reorders the unknowns so that each of them is found once as last one.
         We need a loop over all unknowns. */
    const tbv_unknownVariable_t * const unknownAry = les_getTableOfUnknowns(pLES, NULL);

    boolean success = true
          , storeDet = true
          , isSignOfDetInv = true;
    unsigned int idxUnknown;
    for(idxUnknown=0; success && idxUnknown<noUnknowns; ++idxUnknown)
    {
        if(!pSol->pIsDependentAvailableAry[idxUnknown])
        {
            LOG_INFO( _log
                    , "Unknown %s (%u) is not required for the final result(s) and hence"
                      " not computed. Its value is set to null"
                    , unknownAry[idxUnknown].name
                    , idxUnknown
                    )
            continue;
        }
        
        const char * const nameOfUnknown = unknownAry[idxUnknown].name;

        /* Setup the matrix of coefficients so that the coefficients in column m relate the
           the unknown of interest.
             Sign of determinante: Basically, each call of the solver of the LES will
           (re-)produce the same value of the system determinante. Only the sign will be
           toggled each time because each time we setup the LES we exchange one pair of
           columns. We need to keep track of the sign inversion in order to become able to
           store the determinante only once for all dependents. */
        success = les_setupLES(pLES, nameOfUnknown);
        isSignOfDetInv = !isSignOfDetInv;

        /* Run the symbolic solver, which makes an upper triangular matrix from the LES. It
           fails if the system determinant is null. */
        if(success)
        {
            success = solverLES( pLES->A, /* m */ noUnknowns, /* n */ noKnowns + noUnknowns);

            /* Logging can be done even if the solver fails: We could recognize the linear
               dependent equations in the reported last state of the elimination. */
            if(log_checkLogLevel(_log, log_debug))
            {
                LOG_DEBUG( _log
                         , "LES after%s elimination of %s:"
                         , success? "": " aborted"
                         , nameOfUnknown
                         )
                coe_logMatrix( log_debug
                             , pLES->A
                             , /* m */ noUnknowns
                             , /* n */ noKnowns + noUnknowns
                             , pLES->pTableOfVars
                             );
            }
        }

        if(!success)
        {
            LOG_ERROR( _log
                     , "The LES could not be solved. The circuit has an undefined behavior."
                       " Most probable, you have an invalid interconnection of sources,"
                       " current probes and/or op-amps in your circuit"
                     )
        }

        /* Don't do further result evaluation and storage if there's no solution. */
        if(!success)
            break;

        /* The system determinant is the denominator of all result terms. It is stored only
           in the first cycle. (The debug compilation validates identity in subsequent
           runs.) */
        if(storeDet)
        {
            /* The ownership of the coefficient that holds the determinant is moved from
               *pLES to the newly created solution object. The coefficient in *pLES is set
               to the null object in order to inhibit later freeing of the moved
               coefficient object. */
            assert(coe_isCoefAddendNull(pSol->pDeterminant));
            pSol->pDeterminant = pLES->A[noUnknowns-1][noUnknowns-1];
            pLES->A[noUnknowns-1][noUnknowns-1] = coe_coefAddendNull();
            storeDet = false;
        }
        else
        {
#ifdef DEBUG
            /* Validate identity of the system determinant in each repeated solution of the
               LES.
                 Caution: The validation changes the coefficient matrix pLES->A in
               comparison to the PRODUCTION compilation. */

            /* We have to consider that in each loop one pair of columns of the LES is
               swapped, this leads to a sign inversion of the determinant. Here, we
               compensate for this. */
            if(isSignOfDetInv)
                coe_mulConst(pLES->A[noUnknowns-1][noUnknowns-1], /* constant */ -1);

            pLES->A[noUnknowns-1][noUnknowns-1] = coe_diff( pLES->A[noUnknowns-1][noUnknowns-1]
                                                          , pSol->pDeterminant
                                                          );
            assert(coe_isCoefAddendNull(pLES->A[noUnknowns-1][noUnknowns-1]));
#endif
        } /* End if(The very first unknown or any other one?) */

        /* Store the terms of the result of this unknown in the solution object. One such
           term exists for each known.
             The ownership of the coefficients that hold the terms is moved from *pLES to
           the newly created solution object. The coefficients in *pLES are set to null
           objects in order to inhibit later freeing of the moved coefficient objects. */
        unsigned int idxKnown;
        const unsigned int idxUnknownInSol = pTabOfVars->unknownLookUpAry[idxUnknown].idxCol;
        assert(strcmp(pTabOfVars->unknownLookUpAry[idxUnknown].name, nameOfUnknown) == 0);
        for(idxKnown=0; idxKnown<noKnowns; ++idxKnown)
        {
            pSol->numeratorAry[idxUnknownInSol][idxKnown] =
                                                    pLES->A[noUnknowns-1][noUnknowns+idxKnown];
            pLES->A[noUnknowns-1][noUnknowns+idxKnown] = coe_coefAddendNull();

            /* The LES had been set up with all terms on one side and the condition sum of
               all is null. This means that the coefficients of the numerators need to be
               sign inverted in the result.
                 We have to consider also that in each loop one pair of columns of the LES is
               swapped, which leads to a sign inversion of the determinant. We have stored
               the determinant of the first loop as common denominator, so need to do a
               sign inversion in every other cycle.
                 Putting both together we need to do the inversion in the first and then in
               every second cyle. */
            if(!isSignOfDetInv)
                coe_mulConst(pSol->numeratorAry[idxUnknownInSol][idxKnown], /* constant */ -1);
        }
        
    } /* End for(All required unknowns of the LES) */

    assert(!success || !storeDet);

    /* The additional results are user defined voltages. These voltages are differences of
       node voltages, which can easily be the same as an already found unknown. The
       difference of any pair of node voltages is defined in the same arithmetic space as
       the unknowns so far, so we can compute and store the additional results just like
       that. In particular, the denominator is the same and doesn't need any attention. */
    unsigned int idxSolution, idxUserDefVoltage;
    for( idxUserDefVoltage = 0, idxSolution = noUnknowns
       ; success &&  idxUserDefVoltage < noUserDefVoltages
       ; ++idxUserDefVoltage, ++idxSolution
       )
    {
        const pci_voltageDef_t * const pVoltageDef =
                        &pTabOfVars->pCircuitNetList->voltageDefAry[idxUserDefVoltage];

        if(!pSol->pIsDependentAvailableAry[idxSolution])
        {
            LOG_INFO( _log
                    , "User defined voltage %s (%u) is not required for the final result(s)"
                      " and hence not computed. Its value is set to null"
                    , pVoltageDef->name
                    , idxUserDefVoltage
                    )
            continue;
        }
        
        /* Find the index of the already stored solutions of those nodes' voltages, which
           are defined to be plus and minus pole of the voltage of interest. Both of them
           can be the ground node, in which case no solution is stored. This is indicated
           by index UINT_MAX. */
        const tbv_unknownVariable_t *pUnknown = tbv_getUnknownByNode
                                                 ( pTabOfVars
                                                 , /* idxNode */ pVoltageDef->idxNodePlus
                                                 );
        const unsigned int idxUnknownPlus = pUnknown != NULL? pUnknown->idxCol: UINT_MAX;

        pUnknown = tbv_getUnknownByNode( pTabOfVars
                                       , /* idxNode */ pVoltageDef->idxNodeMinus
                                       );
        const unsigned int idxUnknownMinus = pUnknown != NULL? pUnknown->idxCol: UINT_MAX;

        /* Compute the voltage as difference of the numerators of all known-related terms. */
        unsigned int idxKnown;
        for(idxKnown=0; idxKnown<noKnowns; ++idxKnown)
        {
            assert(pSol->pIsDependentAvailableAry[idxSolution]);
            
            /* The positive node potential is brought into the new result by deep copy. */
            if(idxUnknownPlus != UINT_MAX)
            {
                assert(pSol->pIsDependentAvailableAry[idxUnknownPlus]);
                pSol->numeratorAry[idxSolution][idxKnown] =
                            coe_cloneByDeepCopy(pSol->numeratorAry[idxUnknownPlus][idxKnown]);
            }
            else
            {
                /* Plus pole of user voltage is ground, i.e. we set the first operand of
                   the difference to null. */
                pSol->numeratorAry[idxSolution][idxKnown] = coe_coefAddendNull();
            }

            /* The negative node potential is subtracted in place. */
            if(idxUnknownMinus != UINT_MAX)
            {
                assert(pSol->pIsDependentAvailableAry[idxUnknownMinus]);
                pSol->numeratorAry[idxSolution][idxKnown] =
                                    coe_diff( pSol->numeratorAry[idxSolution][idxKnown]
                                            , pSol->numeratorAry[idxUnknownMinus][idxKnown]
                                            );
            }
            else
            {
                /* Minus pole of user voltage is ground, we have to subtract null, which
                   means doing nothing. */
            }
        }
    } /* End for(All user defined voltages are computed as node differential potentials) */

    if(!success)
    {
        sol_deleteSolution(pSol);
        pSol = NULL;
    }

    /* Return the new, completed object (or NULL). */
    *ppSolution = pSol;
    return success;

} /* End of sol_createSolution */




/**
 * Request another reference to an existing object. The new reference is counted internally
 * for later and safe control of the delete operation.\n
 *   Any requested reference needs to be freed with \a sol_deleteSolution after use.
 *   @return
 * A copy of the passed pointer \a pExistingObj is returned.
 *   @param pExistingObj
 * Pointer to the object to be cloned by reference.
 *   @see const sol_solution_t *sol_cloneByConstReference(const sol_solution_t * const)
 */

sol_solution_t *sol_cloneByReference(sol_solution_t * const pExistingObj)
{
    assert(pExistingObj != NULL);
    ++ pExistingObj->noReferencesToThis;

#ifdef DEBUG
    ++ _noRefsToObjects;
#endif

    return pExistingObj;

} /* End of sol_cloneByReference */





/**
 * Request another reference to an existing constant object. The new reference is counted
 * internally for later and safe control of the delete operation.\n
 *   Any requested reference needs to be freed with \a sol_deleteSolution after use.
 *   @return
 * A read-only copy of the passed pointer \a pExistingObj is returned.
 *   @param pExistingObj
 * Pointer to the (read-only) object to be cloned by reference.
 *   @see boolean sol_createSolution(log_hLogger_t, const sol_solution_t ** const,
 * les_linearEquationSystem_t * const)
 *   @see sol_solution_t *sol_cloneByReference(sol_solution_t * const)
 *   @see void sol_deleteSolution(const sol_solution_t * const)
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

const sol_solution_t *sol_cloneByConstReference(const sol_solution_t * const pExistingObj)
{
    return sol_cloneByReference((sol_solution_t*)pExistingObj);

} /* End of sol_cloneByConstReference */





/**
 * Delete a solution object, which had been created by boolean
 * sol_createSolution(log_hLogger_t, const sol_solution_t ** const,
 * les_linearEquationSystem_t * const).
 *   @param pConstSolution
 * The pointer to the object to delete.
 */

void sol_deleteSolution(const sol_solution_t * const pConstSolution)
{
    /* Tolerate the deletion of the result of unsuccessful object creation. */
    if(pConstSolution == NULL)
        return;

    /* See discussion of const declaration at tbv_cloneByConstReference. The idea of const
       objects requires that we hurt the type qualifier locally here. */
    sol_solution_t * const pSolution = (sol_solution_t*)pConstSolution;

    /* Deletion takes place only if there are no known other references to this object. */
    assert(pSolution->noReferencesToThis >= 1);
    if(--pSolution->noReferencesToThis == 0)
    {
        const tbv_tableOfVariables_t * const pTabOfVars = pSolution->pTableOfVars;

        /* The user could have define some additional voltages of interest. */
        const unsigned int noUserDefVoltages = pTabOfVars->pCircuitNetList->noVoltageDefs;

        coe_freeCoef(pSolution->pDeterminant);
        coe_deleteMatrix( pSolution->numeratorAry
                        , pTabOfVars->noUnknowns + noUserDefVoltages
                        , pTabOfVars->noKnowns
                        );
        tbv_deleteTableOfVariables((tbv_tableOfVariables_t*)pTabOfVars);
        free((boolean*)pSolution->pIsDependentAvailableAry);
        free((sol_solution_t*)pSolution);
    }

#ifdef DEBUG
    -- _noRefsToObjects;
#endif
} /* End of sol_deleteSolution */





/**
 * Get the number of independents the solution depends on.
 *   @return
 * Get the number of independent quantities.
 *   @param pSolution
 * Pass the solution object by reference.
 */

unsigned int sol_getNoIndependents(const sol_solution_t * const pSolution)
{
    return pSolution->pTableOfVars->noKnowns;

} /* End of sol_getNoIndependents */




/**
 * Get the name of an independent quantity of the related solution by index.
 *   @return
 * The name of the independent quantity is returned as read-only string. The character
 * pointer points into the solution object and is valid as long as this object.
 *   @param pSolution
 * The pointer to the object representing the solution.
 *   @param idxIndependent
 * The index of the independent. The permitted range can be queried with \a
 * sol_getNoIndependents.
 *   @see unsigned int sol_getNoIndependents(const sol_solution_t * const)
 */

const char *sol_getNameOfIndependent( const sol_solution_t * const pSolution
                                    , unsigned int idxIndependent
                                    )
{
    assert(idxIndependent < pSolution->pTableOfVars->noKnowns);
    return pSolution->pTableOfVars->knownLookUpAry[idxIndependent].name;

} /* End of sol_getNameOfIndependent */




/**
 * Get the number of dependent quantities, a solution object offers a solution for. This
 * number includes the original unknowns of the LES plus the dependent, user requested
 * unknowns (i.e. user defined voltages).
 *   @return
 * Get the number of distinct solutions in this object. This is at the same time the number
 * of dependent quantities and therefore spawns the index range in \a sol_getNameOfDependent.
 *   @param pSolution
 * Pass the solution object by reference.
 *   @see const char *sol_getNameOfDependent(const sol_solution_t * const, unsigned int)
 */

unsigned int sol_getNoDependents(const sol_solution_t * const pSolution)
{
    const tbv_tableOfVariables_t * const pTabOfVars = pSolution->pTableOfVars;

    /* The user could have define some additional voltages of interest. */
    const unsigned int noUserDefVoltages = pTabOfVars->pCircuitNetList->noVoltageDefs;

    return pTabOfVars->noUnknowns + noUserDefVoltages;

} /* End of sol_getNoDependents */




/**
 * Search a solution for a quantity by name, regardless whether it is an original unknown
 * of the LES, a user-defined voltage or a known of the LES. Get either the indexes of an
 * unknown or user defined voltage and the related solution or the index of a known.\n
 *   The combined search is possible since names need to be globally unique, i.e. across
 * the type of an object. The method may also be used to double-check a name for
 * ambiguity.\n
 *   The indexes are returned as signed integers. A value of -1 means that the object is
 * not of that kind.\n
 *   The indexes are returned by reference. Each index result is optional. Pass the NULL
 * pointer if the specific index result is out of interest.
 *   @return
 * Get the number of matches. Any number other than 1 indicates a kind of problem. The
 * first match is returned in the index pointers if multiple matches are reported.
 *   @param pSolution
 * Pass the solution object by reference.
 *   @param pIdxSolution
 * The index of the related solution is placed in * \a pIdxSolution.
 *   @param pIdxUnknown
 * The index of an unknown would be placed in * \a pIdxUnknown if \a nameOfQuantity
 * designates an unknown. If so, * \a pIdxSolution is also set.
 *   @param pIdxUserDefVoltage
 * The index of a user-defined voltage would be placed in * \a pIdxUserDefVoltage if \a
 * nameOfQuantity designates a user-defined unknown voltage. If so, * \a pIdxSolution is
 * also set.
 *   @param pIdxKnown
 * The index of a known would be placed in * \a pIdxKnown if \a nameOfQuantity designates
 * a known. If so, * \a pIdxSolution is set to -1.
 *   @param nameOfQuantity
 * The known, unknown or user-defined voltage is identified by name. An invalid name will
 * lead to return value 0.
 *   @param doErrorReporting
 * If this flag is set then problems finding the quantity are reported to the global
 * application log. Ambiguous or not matching names are reported. Pass \a false if the
 * function should indicate these kinds of problems by its return value only.
 */

unsigned int sol_findName( const sol_solution_t * const pSolution
                         , signed int * const pIdxSolution
                         , signed int * const pIdxUnknown
                         , signed int * const pIdxUserDefVoltage
                         , signed int * const pIdxKnown
                         , const char * const nameOfQuantity
                         , boolean doErrorReporting
                         )
{
    unsigned int noMatches = 0;
    signed int idxSolution       = -1
             , idxUnknown        = -1
             , idxUserDefVoltage = -1
             , idxKnown          = -1;

    const tbv_tableOfVariables_t * const pTabOfVars = pSolution->pTableOfVars;

    /* The distinct solutions for the unknowns use a linear index first for the true
       unknowns of the original LES then for the user defined voltages. We need to do a two
       stage search. */
    signed int idx;
    for(idx=0; (unsigned)idx<pTabOfVars->noUnknowns; ++idx)
    {
        if(strcmp(pTabOfVars->unknownLookUpAry[idx].name, nameOfQuantity) == 0)
        {
            if(noMatches == 0)
            {
                idxUnknown = idx;
                idxSolution = idx;
            }
            ++ noMatches;
        }
    }
    const unsigned int noUserDefVoltages = pTabOfVars->pCircuitNetList->noVoltageDefs;
    for(idx=0; (unsigned)idx<noUserDefVoltages; ++idx)
    {
        if(strcmp(pTabOfVars->pCircuitNetList->voltageDefAry[idx].name, nameOfQuantity) == 0)
        {
            if(noMatches == 0)
            {
                idxUserDefVoltage = idx;
                idxSolution = (signed int)pTabOfVars->noUnknowns + idx;
            }
            ++ noMatches;
        }
    }
    for(idx=0; (unsigned)idx<pTabOfVars->noKnowns; ++idx)
    {
        if(strcmp(pTabOfVars->knownLookUpAry[idx].name, nameOfQuantity) == 0)
        {
            if(noMatches == 0)
                idxKnown = idx;

            ++ noMatches;
        }
    }

    assert(_log != LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT);
    if(noMatches != 1  && doErrorReporting && log_checkLogLevel(_log, log_error))
    {
        const char *const cause = noMatches > 1
                                  ? "This name is ambiguous. (Forbidden"
                                    " name ambiguities include clashes between"
                                    " dependent and independent quantities.)"
                                  : "No such quantity is defined.";
        log_log( _log
               , log_error
               , "A solution refers to quantity %s. %s"
                 " The list of quantities, which can be referenced:\n"
                 "  Dependent quantities:\n"
               , nameOfQuantity
               , cause
               );
        unsigned int noQuantities = sol_getNoDependents(pSolution);
        unsigned int idxQuantity;
        for(idxQuantity=0; idxQuantity<noQuantities; ++idxQuantity)
        {
            log_log( _log
                   , log_continueLine
                   , "    %s\n"
                   , sol_getNameOfDependent(pSolution, idxQuantity)
                   );
        }
        log_log(_log, log_continueLine, "  Independent quantities:\n");
        noQuantities = sol_getNoIndependents(pSolution);
        for(idxQuantity=0; idxQuantity<noQuantities; ++idxQuantity)
        {
            log_log( _log
                   , log_continueLine
                   , "    %s\n"
                   , sol_getNameOfIndependent(pSolution, idxQuantity)
                   );
        }
    } /* End if(Error reporting needed and demanded?) */

    if(pIdxSolution != NULL)
        *pIdxSolution = idxSolution;
    if(pIdxUnknown != NULL)
        *pIdxUnknown = idxUnknown;
    if(pIdxUserDefVoltage != NULL)
        *pIdxUserDefVoltage = idxUserDefVoltage;
    if(pIdxKnown != NULL)
        *pIdxKnown = idxKnown;

    return noMatches;

} /* End of sol_findName */





/**
 * Get the name of an dependent by index of the related solution.
 *   @return
 * Get the read-only string. The character pointer points into the solution object and is
 * valid as long as this object.
 *   @param pSolution
 * The pointer to the object representing the solution.
 *   @param idxSolution
 * The name of the dependent is returned, which the solution having this index is related
 * to.
 */

const char *sol_getNameOfDependent( const sol_solution_t * const pSolution
                                  , unsigned int idxSolution
                                  )
{
    if(idxSolution < pSolution->pTableOfVars->noUnknowns)
        return pSolution->pTableOfVars->unknownLookUpAry[idxSolution].name;
    else
    {
        idxSolution -= pSolution->pTableOfVars->noUnknowns;
        assert(idxSolution < pSolution->pTableOfVars->pCircuitNetList->noVoltageDefs);
        return pSolution->pTableOfVars->pCircuitNetList->voltageDefAry[idxSolution].name;
    }
} /* End of sol_getNameOfDependent */




/**
 * Print the solution of the linear equation system to the global application log.
 *   @param pSolution
 * The pointer to the object representing the solution to print. The object is  the result
 * of a successful call of boolean sol_createSolution(log_hLogger_t, const sol_solution_t
 * ** const, les_linearEquationSystem_t * const).
 *   @param logLevel
 * The log level at which the output becomes visible. No output is created if the logger
 * object in use has a higher level.
 */

void sol_logSolution(const sol_solution_t *pSolution, log_logLevel_t logLevel)
{
    assert(pSolution != NULL);

    /* All logging is done at given level; we can take a shortcut if this verbosity is not
       desired. */
    if(!log_checkLogLevel(_log, logLevel))
        return;

    const tbv_tableOfVariables_t * const pTableOfVars = pSolution->pTableOfVars;
    const pci_circuit_t * const pCircuitNetList = pTableOfVars->pCircuitNetList;
    const unsigned int noKnowns = pTableOfVars->noKnowns
                     , noUnknowns = pTableOfVars->noUnknowns
                     , noUserVoltages = pCircuitNetList->noVoltageDefs;
    const tbv_knownVariable_t * const knownAry = pTableOfVars->knownLookUpAry;
    const tbv_unknownVariable_t * const unknownAry = pTableOfVars->unknownLookUpAry;

    log_log( _log
           , logLevel
           , "The solution of the linear equation system (%u,%u). All constants have"
             " the physical meaning of (complex) conductance:"
           , noUnknowns
           , noUnknowns + noKnowns
           );

    /* The numerator terms of all dependents are printed first. Later we'll print the common
       denominator just once. */
    unsigned int idxDependent;
    for(idxDependent=0; idxDependent<noUnknowns+noUserVoltages; ++idxDependent)
    {
        const char *nameOfDependent;
        if(idxDependent < noUnknowns)
            nameOfDependent = unknownAry[idxDependent].name;
        else
            nameOfDependent = pCircuitNetList->voltageDefAry[idxDependent-noUnknowns].name;

        log_log(_log, log_continueLine, "\n  %s = ", nameOfDependent);
        if(pSolution->pIsDependentAvailableAry[idxDependent])
        {
            if(noKnowns > 0)
            {
                unsigned int idxKnown;
                for(idxKnown=0; idxKnown<noKnowns; ++idxKnown)
                {
                    log_log( _log
                           , log_continueLine
                           , "%sN_%s/D * %s"
                           , idxKnown == 0? "": " + "
                           , knownAry[idxKnown].name
                           , knownAry[idxKnown].name
                           );
                }
                log_log(_log, log_continueLine, ":", nameOfDependent);

                for(idxKnown=0; idxKnown<noKnowns; ++idxKnown)
                {
                    log_log(_log, log_continueLine, "\n    N_%s = ", knownAry[idxKnown].name);
                    coe_logCoefficient( pSolution->numeratorAry[idxDependent][idxKnown]
                                      , pTableOfVars
                                      , /* tabPos */ 4 + (2+strlen(knownAry[idxKnown].name)) + 3
                                      );
                }
            }
            else
                log_log(_log, log_continueLine, "0");
        }
        else
            log_log(_log, log_continueLine, "(not available)");
        
    } /* for(Output of numerator for all dependents and user defined voltages) */

    /* Print the common denominator of all terms. */
    log_log(_log, log_continueLine, "\n  %sD = ", noKnowns==0? "System determinant ": "");

    coe_logCoefficient(pSolution->pDeterminant, pTableOfVars, /* tabPos */ 6);
    log_log(_log, log_continueLine, "\n");
    log_flush(_log);

} /* End of sol_logSolution */




