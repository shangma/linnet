#ifndef COE_COEFFICIENT_INCLUDED
#define COE_COEFFICIENT_INCLUDED
/**
 * @file coe_coefficient.h
 *   Definition of global interface of module coe_coefficient.c (besides inline interface,
 * see coe_coefficient.inlineInterface.h)
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

#include "lin_linNet.h"
#include "log_logger.h"
#include "mem_memoryManager.h"
#include "crm_createMatrix.h"


/*
 * Defines
 */

/** The maximum number of symbolic constants, which is about the maximum number of connected
    devices. */
#define COE_MAX_NO_CONST  (sizeof(coe_productOfConst_t)*8)


/*
 * Global type definitions
 */

/** Part of the coefficient or an addend of a coefficient: The product of constants. As all
    constants occur either with power 0 or 1, the product is represented by a bit vector,
    where each bit means another constant.\n
      Any one out of the four unsigned integer types char, short, long and long long can be
    used. The performance differences are however minor so that there's no good raeson not
    to always use the 64 Bit type. Here's a performance measurement of a quite complex
    circuit (configuration PRODUCTION):\n
      win32, short:     5091 ms\n
      win32, long:      5206 ms\n
      win32, long long: 5755 ms\n 
      win64, short:     6187 ms\n
      win64, long:      6273 ms\n
      win64, long long: 6646 ms */ 
typedef unsigned long long coe_productOfConst_t;


/** Part of the coefficient or an addend of a coefficient: The numeric constant, usually
    only 1 or -1. */
typedef signed long coe_numericFactor_t;


/** One addend of a coefficient of a linear equation. */
typedef struct coe_coefAddend_t
{
    /** The coefficient is a linked list of these addends. The link to the next addend is
        made by this pointer. NULL for the last or a single addend.
          @remark It's essential to have this member as the very first one. Its address
        needs to be identical to the address of the entire struct. */ 
    struct coe_coefAddend_t *pNext;
    
    /** The numeric factor of the product of constants. Normally either 1 or -1. */
    coe_numericFactor_t factor;
    
    /** A product of constants. Each set bit is related to one multiplied constant. */
    coe_productOfConst_t productOfConst;
   
} coe_coefAddend_t;


/** A complete coefficient of a linear equation. Actually identical to one of its addends
    as a coefficient is a linked list of such addends. The other type name is mainly used
    for code readability: The heading addend represents the complete coefficient at the
    same time. */
typedef coe_coefAddend_t coe_coef_t;


/** The only allowed relation ship of physical constants is an integer multiple of
    each other. The type of factor is defined here. */
typedef signed long coe_integerFactor_t;


/** The matrix A[m*n] is defined as array of pointers to rows. This permits dynamic
    allocation of matrixes of any size. */
typedef coe_coef_t ***coe_coefMatrix_t;


/** A forward declaration to an external type in order to avoid crosswise includes. Never
    use this type. */
struct tbv_tableOfVariables_t;


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */

/** Initialize the module prior to the first use of any of its operations or global data
    objects. */
void coe_initModule(log_hLogger_t hGlobalLogger);

/** Do all cleanup after use of the module, which is required to avoid memory leaks,
    orphaned handles, etc. */
void coe_shutdownModule(void);

/** Make a complete copy of all the addends of a coefficient. */
coe_coef_t *coe_cloneByDeepCopy(const coe_coef_t *pCoef);

#if 1
CRM_DECLARE_CREATE_MATRIX(/* modulePrefix */  coe, /* elementType_t */ coe_coef_t*)
#else // For documentation purpose:
/** Create a rectangular matrix of null coefficients. */
coe_coefMatrix_t coe_createMatrix(unsigned int noRows, unsigned int noCols);

/** Delete a matrix of coefficients after use. */
void coe_deleteMatrix(coe_coefMatrix_t A, unsigned int noRows, unsigned int noCols);
#endif

/** This function double checks if a coefficient object's addends have the demanded order. */
boolean coe_checkOrderOfAddends(coe_coef_t *pCoef);

/** Sort the order of addends in a (new) coefficient object prior to its first use. */
void coe_sortAddends(coe_coef_t ** const ppCoef);

/** Log a single coefficient of the LES. */
void coe_logCoefficient( const coe_coef_t * const pCoeff
                       , const struct tbv_tableOfVariables_t * const pTableOfVars
                       , unsigned int tabPos
                       );

/** Print the a matrix of coefficients to the application log. */
void coe_logMatrix( log_logLevel_t logLevel
                  , const coe_coefMatrix_t A
                  , unsigned int m
                  , unsigned int n
                  , const struct tbv_tableOfVariables_t * const pTableOfVars
                  );

/** Multiply a coefficient with an integer constant. */
coe_coef_t *coe_mulConst(coe_coef_t * const pCoef, signed int constant);

/** SUbtract two coefficients. */
coe_coef_t *coe_diff(coe_coef_t *pResOp1, const coe_coef_t * const pOp2);

#endif  /* COE_COEFFICIENT_INCLUDED */
