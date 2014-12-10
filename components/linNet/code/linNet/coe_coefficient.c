/**
 * @file coe_coefficient.c
 *   Basic data structures and types for handling the coefficients of the linear equation
 * system. The coefficients are sums of products of (physical) constants.
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
/* Module interface
 *   coe_initModule
 *   coe_shutdownModule
 *   coe_cloneByDeepCopy
 *   coe_createMatrix
 *   coe_deleteMatrix
 *   coe_checkOrderOfAddends
 *   coe_sortAddends
 *   coe_logCoefficient
 *   coe_logMatrix
 *   coe_mulConst
 *   coe_diff
 * Local functions
 *   addAddendToExpr
 */

/*
 * Include files
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "smalloc.h"
#include "log_logger.h"
#include "mem_memoryManager.h"
#include "tbv_tableOfVariables.h"
#include "coe_coefficient.h"
#include "coe_coefficient.inlineInterface.h"


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

/** The module globally accessible heap for both the coefficients of the LES and the
    addends of a coefficient.
      @remark Although defined globally, nobody should ever use this variable directly.
    Instead, use the functional interface declared in coe_coefficient.inlineInterface.h to
    do so. The inline implementation of this function set demands to declare the memory
    manager as a global. */
mem_hHeap_t coe_hHeapOfCoefAddend = MEM_HANDLE_INVALID_HEAP;


/*
 * Function implementation
 */


/**
 * Add a single coefficient addend to a coefficient.
 *   @param ppCoef
 * The manipulated coefficient is passed as a pointer to the pointer of its head addend. May
 * be a null coefficent.
 *   @param pNewAddend
 * The addend to add is passed by reference. It must not be a null addend. The manipulated
 * coefficient takes ownership of the object, the caller must no longer use or reference it
 * after return. Particularly, he must not free the addend object.
 */ 

static void addAddendToExpr(coe_coef_t ** const ppCoef, coe_coefAddend_t * const pNewAddend)
{
    assert(!coe_isCoefAddendNull(pNewAddend));

    coe_coefAddend_t *pAddend = *ppCoef
                   , **ppAddend = ppCoef;
    const coe_productOfConst_t productOfConsts = pNewAddend->productOfConst;

    /* The addends of a coefficient are sorted. Look for the position where to combine with
       an existing addend or where to insert a new one. */
    while(!coe_isCoefAddendNull(pAddend) &&  pAddend->productOfConst > productOfConsts)
    {
        ppAddend = &pAddend->pNext;
        pAddend = pAddend->pNext;
    }

    if(coe_isCoefAddendNull(pAddend) ||  pAddend->productOfConst < productOfConsts)
    {
        /* The coefficient doesn't contain an addend with identical combination of
           constants. We insert the passed addend as a new one. */
        *ppAddend = pNewAddend;
        pNewAddend->pNext = pAddend;
    }
    else
    {
        /* The coefficient already contains an addend with identical combination of
           constants. We combine this addend with the new one: the factors are simply
           added. */
        if((pAddend->factor+=pNewAddend->factor) == 0)
        {
            /* The sum of the factors is null, the addend is eliminated. Remove the list
               element from the coefficient. Furthermore, the passed, new addend needs to
               be eliminated. */
            (*ppAddend) = pAddend->pNext;
            coe_freeCoefAddend(pAddend);
            coe_freeCoefAddend(pNewAddend);
        }
    }
} /* End of addAddendToExpr */




/**
 * Initialize the module at application startup.\n
 *   Mainly used to initialize golbally accessible heap for LES coefficient objects.
 *   @param hGlobalLogger
 * This module will use the passed logger object for all reporting during application life
 * time.
 *   @remark
 * Do not forget to call the counterpart at application end.
 *   @see void coe_shutdownModule()
 */

void coe_initModule(log_hLogger_t hGlobalLogger)
{
    /* Use the passed logger during the module life time. */
    _log = log_cloneByReference(hGlobalLogger);

    /* Initialize the global heap of coefficients. */
    coe_hHeapOfCoefAddend = mem_createHeap( hGlobalLogger
                                          , /* Name */ "Coefficient of LES"
                                          , sizeof(coe_coefAddend_t)
                                          , /* initialHeapSize */     1000
                                          , /* allocationBlockSize */ 10000
                                          );
#ifdef DEBUG
   coe_coefAddend_t dummyObj;
   assert((char*)&dummyObj.pNext == (char*)&dummyObj + MEM_OFFSET_OF_LINK_POINTER
          &&  sizeof(dummyObj.pNext) == MEM_SIZE_OF_LINK_POINTER
         );
#endif
} /* End of coe_initModule */




/**
 * Do all cleanup after use of the module, which is required to avoid memory leaks, orphaned
 * handles, etc.
 */

void coe_shutdownModule()
{
    /* The DEBUG compilation looks for still allocated objects in order to detect memory
       leaks. */
#ifdef DEBUG
    mem_deleteHeap(coe_hHeapOfCoefAddend, /* warnIfUnfreedMem */ true);
#else
    mem_deleteHeap(coe_hHeapOfCoefAddend, /* warnIfUnfreedMem */ false);
#endif
    coe_hHeapOfCoefAddend = MEM_HANDLE_INVALID_HEAP;

    /* Invalidate the reference to the passed logger. It must no longer be used. */
    log_deleteLogger(_log);
    _log = LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT;

} /* End of coe_shutdownModule */





/**
 * A coefficient object is entirely copied, i.e. a single addend must not be passed.
 *   @return
 * The copy is returned.
 *   @param pCoef
 * The coefficient to be copied. The addend the \a pCoef points to and all its successors
 * (linked by pointer pNext) are copied.
 */

coe_coef_t *coe_cloneByDeepCopy(const coe_coef_t *pCoef)
{
    /* To unify the list operations in the later loop, we begin the list with an arbitray
       head element, whose only quality of interest is that it has a next pointer. */
    coe_coef_t *pHeadOfCopy = coe_newCoefAddend();
    
    coe_coefAddend_t **ppNext = &pHeadOfCopy->pNext;

    while(!coe_isCoefAddendNull(pCoef))
    {
        coe_coefAddend_t *pNewAddend = coe_newCoefAddend();
        /* pNewAddend->pNext is set in either the next loop or behind it. */
        pNewAddend->factor = pCoef->factor;
        pNewAddend->productOfConst = pCoef->productOfConst;
        
        *ppNext = pNewAddend;
        
        pCoef = pCoef->pNext;
        ppNext = &pNewAddend->pNext;

    } /* End while(All addends of the coefficient) */
    
    /* End the list with a next pointer to the null term. */
    (*ppNext) = coe_coefAddendNull();

    /* Free the dummy head element but return its tail as result. */
    coe_coef_t * const pRes = pHeadOfCopy->pNext; 
    coe_freeCoefAddend(pHeadOfCopy);
    return pRes;

} /* End of coe_cloneByDeepCopy */



/* Generate code for a pair of functions to create and delete a matrix of coefficients. */
CRM_CREATE_MATRIX( /* modulePrefix */        coe
                 , /* elementType_t */       coe_coef_t*
                 , /* initialElementValue */ coe_coefAddendNull()
                 , /* fctFreeElement */      coe_freeCoef
                 )



/**
 * Many operations on coefficients require that the addends are sorted. The order is
 * determined by the product of constants. Taking this product as a binary number, the
 * addend needs to be ordered in falling value of this number. This function double checks
 * if a coefficient object regrads this order.\n
 *   @return
 * \a true if order of addends is okay, \a false otherwise.
 *   @param pCoef
 * * \a pCoef is the double-checked coefficient object.
 */

boolean coe_checkOrderOfAddends(coe_coef_t *pCoef)
{
    if(!coe_isCoefAddendNull(pCoef))
    {
        assert(pCoef->factor == 1  ||  pCoef->factor == -1);
        coe_productOfConst_t productBefore = pCoef->productOfConst;
        pCoef = pCoef->pNext;
        while(!coe_isCoefAddendNull(pCoef))
        {
            assert(pCoef->factor == 1  ||  pCoef->factor == -1);
            if(productBefore <= pCoef->productOfConst)
                return false;
                
            productBefore = pCoef->productOfConst;
            pCoef = pCoef->pNext;
        }
    }
   
    return true;

} /* End of coe_checkOrderOfAddends */




/**
 * Many operations on coefficients require that the addends are sorted. The order is
 * determined by the product of constants. Taking this product as a binary number, the
 * addends need to be ordered in falling value of this number. If creating a coefficient
 * object is done disregarding this order criterion then this function can be called
 * on the just assembled coefficient object prior to its first use.\n
 *   The sorting algorithm is very simple and of order O(n^2). The use of this function can
 * be avoided in most use cases by creating a new coefficient object as a null object and
 * then adding its terms with function void coe_addAddend(coe_coef_t **, const
 * coe_numericFactor_t, coe_productOfConst_t). This is more elegant but doesn't perform
 * better (also O(n^2)).
 *   @param ppCoef
 * The function operates in place. The pointer to the head of the list of addends is passed
 * in by reference. Upon return the value of the head pointer will be changed, the list is
 * resorted. No addends need to/must be freed.
 */

void coe_sortAddends(coe_coef_t ** const ppCoef)
{
    coe_coef_t *pResult = coe_coefAddendNull();
    coe_coef_t *pCoef = *ppCoef;
    while(!coe_isCoefAddendNull(pCoef))
    {
        /* The addend is taken from the head of the list, which makes up the coefficient
           and then added to the new object. The add operation considers the required order
           of addends. */
        coe_coefAddend_t *pAddend = pCoef;
        pCoef = pCoef->pNext;
        addAddendToExpr(&pResult, pAddend);
    }
   
    /* Reuse same pointer-to-coef variable to hold the final result. */
    *ppCoef = pResult;

} /* End of coe_sortAddends */




/**
 * Log a single coefficient of the LES. Logging is done regardless of level and without a
 * terminating newline into the application log. The call of this method should always be
 * preceeded by a level related call of log_log and within a conditional clause which
 * implements the same level filter.
 *   @param pCoef
 * Pointer to the coefficient of the LES. It's a list of addends.
 *   @param pTableOfVars
 * The data structure holding all information about the symbolic elements of the LES.
 *   @param tabPos
 * The tabulator position at which to begin to write in wrapped lines.
 */

void coe_logCoefficient( const coe_coef_t * const pCoef
                       , const tbv_tableOfVariables_t * const pTableOfVars
                       , unsigned int tabPos
                       )
{
    /* The number of addends that can be printed in one line depends on the number of
       constants, which can be combined in each addend. */
    const unsigned int maxTermsPerLine = pTableOfVars->noConstants > 10
                                         ? 2
                                         : (pTableOfVars->noConstants > 6
                                            ? 3
                                            : 5
                                           );

    if(coe_isCoefAddendNull(pCoef))
    {
        /* The format of the ouput in case of a null coefficient needs to be done in sync
           with the general output code in the else clause. We could also decide to have a
           +0. */
        log_log(_log, log_continueLine, " 0");
    }
    else
    {
        const coe_coefAddend_t *pAddend = pCoef;
        unsigned int t = 0;
        do
        {
            coe_integerFactor_t i;
            boolean firstTerm = true;

            /* Prepare a string, which is used to indent the second, third, etc. line of
               output. */
            char tabStr[tabPos+1];
            memset(tabStr, /* value */ ' ', /* noBytes */ tabPos);
            tabStr[tabPos] = '\0';

            /* Write the numerical constant (1 or -1) but force having a sign. */
            i = pAddend->factor;
            log_log(_log, log_continueLine, "%c", i<0? '-': '+');
            if((i != 1 && i != -1) ||  pAddend->productOfConst == 0)
            {
                log_log(_log, log_continueLine, "%ld", i<0? -i: i);
                firstTerm = false;
            }

            signed int idxVar = pTableOfVars->noConstants - 1;
            const coe_productOfConst_t prodOfConst = pAddend->productOfConst;

#if 1 // Set to 0 in order to get a concise binary representation of the product of constants

            coe_productOfConst_t mask = idxVar >= 0? ((coe_productOfConst_t)0x1 << idxVar)
                                                   : (coe_productOfConst_t)0x1;
            assert(mask != 0);
            while(idxVar >= 0)
            {
                if((mask & prodOfConst) != 0)
                {
                    if(!firstTerm)
                        log_log(_log, log_continueLine, "*");
                    firstTerm = false;

                    assert((unsigned)idxVar < pTableOfVars->noConstants);
                    const pci_device_t * const pDev = tbv_getDeviceByBitIndex( pTableOfVars
                                                                             , idxVar
                                                                             );
                    log_log(_log, log_continueLine, "%s", pDev->name);
                }
                
                -- idxVar;
                mask >>= 1;            
            }
#else
            if(!firstTerm)
                log_log(_log, log_continueLine, "*");
            firstTerm = false;
            
            /* The product of constants is not shown as such but its binary representation
               is printed instead. */
            log_log(_log, log_continueLine, "0x%x", prodOfConst);
#endif
            pAddend = pAddend->pNext;

            /* Add a line feed and indentation white space if print margin is exceeded. */
            if(!coe_isCoefAddendNull(pAddend)  &&  ++t >= maxTermsPerLine)
            {
                log_log(_log, log_continueLine, "\n");
                log_log(_log, log_continueLine, tabStr);
                t = 0;
            }
        }
        while(!coe_isCoefAddendNull(pAddend));
        
    } /* End if(Special case of a null coefficient?) */
    
} /* End of coe_logCoefficient */



/**
 * Print the a matrix of coefficients (i.e. the linear equation system) to the application
 * log.
 *   @param logLevel
 * The log level at which the output becomes visible. No output is created if the logger
 * object in use has a higher level.
 *   @param A
 * The matrix of coefficients of the LES. The current contents of the matrix are reported.
 *   @param m
 * Number of rows of A.
 *   @param n
 * Number of columns of A. n >= m+1.
 *   @param pTableOfVars
 * The data structure holding all information about the symbolic elements refered to in the
 * coefficients of the matrix A.
 */

void coe_logMatrix( log_logLevel_t logLevel
                  , const coe_coefMatrix_t A
                  , unsigned int m
                  , unsigned int n
                  , const tbv_tableOfVariables_t * const pTableOfVars
                  )
{
    /* All logging is done at given level; we can take a shortcut if this verbosity is not
       desired. */
    if(!log_checkLogLevel(_log, logLevel))
        return;

    log_logLine(_log, logLevel, "The linear equation system (%u, %u):", m, n);

    unsigned int row;
    for(row=0; row<m; ++row)
    {
        LOG_DEBUG(_log, "  Row %u:", row);
        unsigned int col;
        for(col=0; col<n; ++col)
        {
            log_log(_log, logLevel, "    (%2d,%2d): ", row, col);
            coe_logCoefficient( A[row][col]
                              , pTableOfVars
                              , /* tabPos */ 13 + log_getLengthOfLineHeader(_log)
                              );
            log_log(_log, log_continueLine, "\n");
        }
    }

    log_flush(_log);

} /* End of coe_logMatrix */







/**
 * Take a coefficient times a numeric constant. The operation is done in place.
 *   @return
 * The modified coefficient is returned by reference.
 *   @param pCoef
 * The coefficient to operate on is passed by reference.
 *   @param constant
 * A constant integer not equal to null.
 */

coe_coef_t *coe_mulConst(coe_coef_t * const pCoef, signed int constant)
{
    assert(constant != 0);
    coe_coef_t *pC = pCoef;
    while(!coe_isCoefAddendNull(pC))
    {
#ifdef DEBUG
        const coe_numericFactor_t c = pC->factor;
#endif
        pC->factor *= constant;
        assert(pC->factor / constant == c);
        
        pC = pC->pNext;
    }
   
    return pCoef;
    
} /* End of coe_mulConst */




/**
 * The difference of two coefficients. The second operand is subtracted in place from the
 * first operand.
 *   @return
 * The difference is returned. It is the same list of coefficient addends as passed in as
 * pResOperand1, but the pointer value may differ due to the needed list operations.
 * Consequently, you must never free the coefficient \a *pResOperand1 (but the returned list
 * after use).
 *   @param pResOperand1
 * First operand by reference. It is replaced by the result.
 *   @param pOperand2
 * Second operand by reference. It is not changed.
 */

coe_coef_t *coe_diff(coe_coef_t *pResOperand1, const coe_coef_t * const pOperand2)
{
    /* Each addend of the second operand is either subtracted from an equivalent term in the
       first operand or copied with inverse sign into the first operands list of addends.
       (An equivalent term has the identical product of constants.) */
    const coe_coef_t *pOp2 = pOperand2;
    while(!coe_isCoefAddendNull(pOp2))
    {
        coe_coefAddend_t *pOp1 = pResOperand1
                       , **ppOp1 = &pResOperand1;
        const coe_productOfConst_t prodOfConstOp2 = pOp2->productOfConst;

        /* The addends of a coefficient are ordered by falling value of the bit vector
           "product of constants", where this bitvector is read as an unsigned integer. We
           iterate along the result list until we find the location, where the current
           addend of the second operand belongs. This can be the head or the tail of the
           list or an existing element with identical product of constants. */
        while(!coe_isCoefAddendNull(pOp1) &&  pOp1->productOfConst > prodOfConstOp2)
        {
            ppOp1 = &pOp1->pNext;
            pOp1  = pOp1->pNext;
        }
            
        if(coe_isCoefAddendNull(pOp1) ||  pOp1->productOfConst < prodOfConstOp2)
        {
            /* There's no equivalent addend in the result list. Copy the addend with
               inverse sign from Op2 to the result. */
            *ppOp1 = coe_newCoefAddend();
            (*ppOp1)->pNext = pOp1;
            (*ppOp1)->factor = - pOp2->factor;
            (*ppOp1)->productOfConst = prodOfConstOp2;
        }
        else
        {
            /* An equivalent addent is present in op1, simply add the factors. */
            assert(pOp1->productOfConst == prodOfConstOp2);
            pOp1->factor -= pOp2->factor;

            /* If the difference of factor is null we need to remove the addend from the
               list; a factor 0 of a product is by definition an invalid representation of
               null. */
            if(pOp1->factor == 0)
            {
                *ppOp1 = pOp1->pNext;
                coe_freeCoefAddend(pOp1);
            }
        }

        pOp2 = pOp2->pNext;

    } /* End while(All addends of second, subtracted operand. */
    
    return pResOperand1;
           
} /* End of coe_diff */






