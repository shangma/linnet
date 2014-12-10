#ifndef COE_COEFFICIENT_INLINE_INTERFACE_INCLUDED
#define COE_COEFFICIENT_INLINE_INTERFACE_INCLUDED
/**
 * @file coe_coefficient.inlineInterface.h
 *   Implementation of global inline interface of module coe_coefficient.c.\n
 *   All global inline functions are declared "static inline" in order to avoid any
 * problems with undefined or doubly defined symbols at linkage time. The drawback of this
 * declaration is that any client of this module's header file would instantiate its own
 * set of functions. Most clients of this module's header file won't however use the inline
 * functions. By placing the inline interface in a secondary file, any client can decide
 * whether to include the additional header (and instantiate the functions) or not.
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

#include "types.h"
#include "log_logger.h"
#include "mem_memoryManager.h"
#include "coe_coefficient.h"


/*
 * Defines
 */


/*
 * Global type definitions
 */


/*
 * Global data declarations
 */

/** The module exports a globally usable heap for both the coefficients of the LES and the
    addends of a coefficient.
      @remark In DEBUG compilation and when the heap is destroyed at application
    termination time the heap is checked for still allocated objects. The client of the
    heap should return all requested objects in order to have a memory leak detection in
    DEBUG compilation. */
extern mem_hHeap_t coe_hHeapOfCoefAddend;


/*
 * Global inline functions
 */

/**
 * Return an uninitialized coefficient object. It has been allocated using the
 * global heap mem_hHeap_t coe_hHeapOfCoefAddend and needs to be freed after use with void
 * mem_free(heap_t *, void *).
 *   @return
 * Get a pointer to the new object.
 */

static inline coe_coefAddend_t *coe_newCoefAddend()
{
    return (coe_coefAddend_t*)mem_malloc(coe_hHeapOfCoefAddend);

} /* End of coe_newCoefAddend */




/**
 * Return a coefficient object, that represents null. It has been allocated using the
 * global heap mem_hHeap_t coe_hHeapOfCoefAddend and needs to be freed after use with void
 * mem_free(heap_t *, void *).
 *   @return
 * Get a pointer to the new object.
 */

static inline coe_coefAddend_t *coe_coefAddendNull()
{
    return NULL;

} /* End of coe_coefAddendNull */




/**
 * Return an addent of a coefficient object, that represents 1. It has been allocated using
 * the global heap mem_hHeap_t coe_hHeapOfCoefAddend and needs to be freed after use with
 * void mem_free(heap_t *, void *).
 *   @return
 * Get a pointer to the new object.
 */

static inline coe_coefAddend_t *coe_coefAddendOne()
{
    coe_coefAddend_t *pOne = coe_newCoefAddend();
    pOne->pNext = NULL;
    pOne->factor = 1;
    pOne->productOfConst = 0;
    return pOne;

} /* End of coe_coefAddendOne */



/**
 * Check the addent of a coefficient object if it represents 1.
 *   @return
 * Get the Boolean answer.
 */

static inline boolean coe_isCoefAddendNull(const coe_coefAddend_t * const pAddend)
{
    return pAddend == NULL;

} /* End of coe_isCoefAddendNull */



/**
 * Free a single addent of a coefficient object which had been allocated with coe_coefAddend_t
 * *coe_newCoefAddend(). If the addend is part of a linked list of addends, then the
 * followers of the deleted addend are ignored.
 *   @param pCoefAddend
 * The pointer to the returned (i.e. freed) coefficient addend object.
 */

static inline void coe_freeCoefAddend(coe_coefAddend_t *pCoefAddend)
{
    if(!coe_isCoefAddendNull(pCoefAddend))
        mem_free(coe_hHeapOfCoefAddend, pCoefAddend);

} /* End of coe_freeCoefAddend */



/**
 * Free a complete coefficient, which is a linked list of addends that had been allocated
 * with coe_coefAddend_t *coe_newCoefAddend().
 *   @param pCoef
 * The pointer to the freed coefficient or the pointer to the head of the linked list of
 * addends of those.
 */

static inline void coe_freeCoef(coe_coef_t *pCoef)
{
    if(!coe_isCoefAddendNull(pCoef))
        mem_freeList(coe_hHeapOfCoefAddend, pCoef);

} /* End of coe_freeCoef */



/**
 * Compute the sum of a coefficient and a single addend (i.e. a product of constants and a
 * numeric factor). The operation is done in place.
 *   @param ppCoef
 * The coefficient object by reference. This object is changed in place, i.e. it is
 * replaced by the sum if it and the new addend.
 *   @param factor
 * The new addend is passed as pair of primitive data types. Here the numeric factor, which
 * must not be null.
 *   @param productOfConsts
 * The new addend is passed as pair of primitive data types. Here the product of constants.
 */

static inline void coe_addAddend( coe_coef_t ** const ppCoef
                                , const coe_numericFactor_t factor
                                , coe_productOfConst_t productOfConsts
                                )
{
    assert(factor != 0);
    
    coe_coefAddend_t *pAddend = *ppCoef
                   , **ppAddend = ppCoef;

    /* The addends of a coefficient are sorted. Look for the position where to combine with an
       existing addend or where to insert a new one. */
    while(!coe_isCoefAddendNull(pAddend) &&  pAddend->productOfConst > productOfConsts)
    {
        ppAddend = &pAddend->pNext;
        pAddend = pAddend->pNext;
    }

    if(coe_isCoefAddendNull(pAddend) ||  pAddend->productOfConst < productOfConsts)
    {
        /* The coefficient doesn't contain an addend with identical combination of
           constants. We insert a new addend. */
        *ppAddend = coe_newCoefAddend();
        (*ppAddend)->pNext = pAddend;
        (*ppAddend)->factor = factor;
        (*ppAddend)->productOfConst = productOfConsts;
    }
    else
    {
        /* The coefficient already contains an addend with identical combination of
           constants. We combine this addend with the new one: the factors are simply
           added. */
        if((pAddend->factor+=factor) == 0)
        {
            /* The sum of the factors is null, the addend is eliminated. Remove the list
               element from the coefficient. */
            (*ppAddend) = pAddend->pNext;
            coe_freeCoefAddend(pAddend);
        }
    }
} /* End coe_addAddend. */



/*
 * Global prototypes
 */



#endif  /* COE_COEFFICIENT_INLINE_INTERFACE_INCLUDED */
