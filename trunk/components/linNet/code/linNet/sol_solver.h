#ifndef SOL_SOLVER_INCLUDED
#define SOL_SOLVER_INCLUDED
/**
 * @file sol_solver.h
 * Definition of global interface of module sol_solver.c
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

/*
 * Include files
 */

#include "log_logger.h"
#include "coe_coefficient.h"
#include "tbv_tableOfVariables.h"
#include "les_linearEquationSystem.h"


/*
 * Defines
 */


/*
 * Global type definitions
 */

/** The solution object contains the complete symbolic solution of the LES. */
typedef struct sol_solution_t
{
    /** A counter of references to this object. Used to control deletion of object. */
    unsigned int noReferencesToThis;

    /** The table of variables describes the knowns, unknowns and constants of the problem;
        among more, their names are found here. */
    const tbv_tableOfVariables_t *pTableOfVars;

    /** A Boolean vector indicating which of the dependent quantities have been computed.
        The other solutions in \a numeratorAry are null coefficients. */
    const boolean *pIsDependentAvailableAry;    
    
    /** The determinant of the LES and the denominator of all terms of the solution at the
        same time. */
    coe_coef_t *pDeterminant;
    
    /** An array of numerators of result terms. There's one such term for each combination
        of dependent (i.e. unknown or user defined voltage) and known:
        numeratorAry[idxDependent][idxKnown]. */
    coe_coefMatrix_t numeratorAry;
    
} sol_solution_t;



/*
 * Global data declarations
 */


/*
 * Global prototypes
 */

/** Initialize the module prior to first use of any of its methods or global data objects. */
void sol_initModule(log_hLogger_t hGlobalLogger);

/** Shutdown of module after use. Release of memory, closing files, etc. */
void sol_shutdownModule(void);

/** Compute the solution of a LES and return it as a dedicated object. */
boolean sol_createSolution( const sol_solution_t * * const ppSolution
                          , les_linearEquationSystem_t * const pLES
                          );

/** Get another reference to an existing object. */
sol_solution_t *sol_cloneByReference(sol_solution_t * const pExistingObj);

/** Get another reference to an existing constant object. */
const sol_solution_t *sol_cloneByConstReference(const sol_solution_t * const pExistingObj);

/** Delete a solution object as got from sol_createSolution. */
void sol_deleteSolution(const sol_solution_t * const pConstSolution);

/** Get the number of unknowns, i.e. of independent quantities, a solution object offers a
    solution for. */
unsigned int sol_getNoIndependents(const sol_solution_t * const pSolution);

/** Get the name of a known by index of the related solution. */
const char *sol_getNameOfIndependent( const sol_solution_t * const pSolution
                                    , unsigned int idxIndependent
                                    );

/** Get the number of unknowns, a solution object offers a solution for. */
unsigned int sol_getNoDependents(const sol_solution_t * const pSolution);

/** Get the name of a dependent quantity by index. */
const char *sol_getNameOfDependent( const sol_solution_t * const pSolution
                                  , unsigned int idxDependent
                                  );
                                
/** Search the solution for a quantity by name (an original unknown of the LES, a
    user-defined voltage or a known of the LES). */
unsigned int sol_findName( const sol_solution_t * const pSolution
                         , signed int * const pIdxSolution
                         , signed int * const pIdxUnknown
                         , signed int * const pIdxUserDefVoltage
                         , signed int * const pIdxKnown
                         , const char * const nameOfQuantity
                         , boolean doErrorReporting
                         );

/** Print a complete solution to the application log. */
void sol_logSolution(const sol_solution_t *pSolution, log_logLevel_t logLevel);

#endif  /* SOL_SOLVER_INCLUDED */
