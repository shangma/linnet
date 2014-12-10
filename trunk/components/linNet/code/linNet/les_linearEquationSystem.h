#ifndef LES_LINEAREQUATIONSYSTEM_INCLUDED
#define LES_LINEAREQUATIONSYSTEM_INCLUDED
/**
 * @file les_linearEquationSystem.h
 * Definition of global interface of module les_linearEquationSystem.c
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
#include "coe_coefficient.h"
#include "pci_parserCircuit.h"
#include "tbv_tableOfVariables.h"


/*
 * Defines
 */


/*
 * Global type definitions
 */

/** Forward declaration of a hidden data type. */
struct les_network_s;

/** The data structure that holds the linear equations system that describes the ideal
    linear behavior of the electric circuit. */
typedef struct
{
    /** The matrix of coeffcients of the LES. */
    coe_coefMatrix_t A;

    /** The table of variables and constants, which are used in the LES and its
        coefficients. */
    tbv_tableOfVariables_t *pTableOfVars;
        
    /** The object undergoes some the data validating operations repeatedly. All
        informative and warning output should not be done repeatedly. The next (internal)
        flag indicates to the validation algorithms if output should take place. */
    boolean doWarn;
    
} les_linearEquationSystem_t;


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */

/** Initialize the module prior to the first use of any of its operations or global data
    objects. */
void les_initModule(log_hLogger_t hGlobalLogger);

/** Do all cleanup after use of the module, which is required to avoid memory leaks,
    orphaned handles, etc. */
void les_shutdownModule(void);

/** Create the linear equation system from the net list representing the electric circuit. */
boolean les_createLES( les_linearEquationSystem_t * * const ppLES
                     , const pci_circuit_t * const pCircuitNetList
                     );

/** Delete a linear equation system object after use. */
void les_deleteLES(les_linearEquationSystem_t *pLES);

/** Get the dimensions of the created LES. */
void les_getNoVariables( const les_linearEquationSystem_t * const pLES
                       , unsigned int * const pNoKnowns
                       , unsigned int * const pNoUnknowns
                       , unsigned int * const pNoConstants
                       );

/** Get the table of symbols (knowns, unknowns, constants). */
const tbv_tableOfVariables_t *les_getTableOfVariables
                                        (const les_linearEquationSystem_t * const pLES); 

/** Get the table of unknowns. */
const tbv_unknownVariable_t *les_getTableOfUnknowns
                                        ( const les_linearEquationSystem_t * const pLES
                                        , unsigned int * const pNoUnknowns
                                        );
                                        
/** Setup the LES (compute its coefficients), so that the solver can compute the solution
    for a specific unknown. */
boolean les_setupLES( les_linearEquationSystem_t * const pLES
                    , const char * const nameOfUnknown
                    );

#endif  /* LES_LINEAREQUATIONSYSTEM_INCLUDED */
