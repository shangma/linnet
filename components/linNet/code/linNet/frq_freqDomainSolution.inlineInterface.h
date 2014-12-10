#ifndef FRQ_FREQDOMAINSOLUTION_INLINEINTERFACE_INCLUDED
#define FRQ_FREQDOMAINSOLUTION_INLINEINTERFACE_INCLUDED
/**
 * @file frq_freqDomainSolution.inlineInterface.h
 *   Implementation of global inline interface of module frq_freqDomainSolution.c.\n
 *   All global inline functions are declared "static inline" in order to avoid any
 * problems with undefined or doubly defined symbols at linkage time. The drawback of this
 * declaration is that any client of this module's header file would instantiate its own
 * set of functions. Most clients of this module's header file won't however use the inline
 * functions. By placing the inline interface in a secondary file, any client can decide
 * whether to include the additional header (and instantiate the functions) or not.
 *
 * Copyright (C) 2014 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *   frq_getIsBodePlot
 *   frq_getResultName
 *   frq_getNoIndependents
 *   frq_getNoDependents
 */

/*
 * Include files
 */

#include "pci_parserCircuit.h"
#include "tbv_tableOfVariables.h"
#include "frq_freqDomainSolution.h"


/*
 * Defines
 */


/*
 * Global type definitions
 */


/*
 * Global data declarations
 */


/*
 * Global inline functions
 */



/**
 * Get the Boolean information whether this is a Bode plot, or a full result otherwise.
 *   @return
 * true for Bode plot, false for a full result.
 *   @param pSolution
 * The pointer to the solution object.
 */

static inline boolean frq_getIsBodePlot(const frq_freqDomainSolution_t * const pSolution)
{
    boolean isBodePlot;
    const tbv_tableOfVariables_t * const pTableOfVars = pSolution->pTableOfVars;
    if(pSolution->idxResult >= 0)
    {
        /* Normal situation: This object is related to a user defined result with a sub-set
           of unknowns and/or user defined voltages as dependents. */
        assert((unsigned)pSolution->idxResult < pTableOfVars->pCircuitNetList->noResultDefs);
        const pci_resultDef_t * const pResultDef = &pTableOfVars
                                                    ->pCircuitNetList
                                                      ->resultDefAry[pSolution->idxResult];
                                                      
        /* Here, we can make the distinction between a full result and a Bode plot. */
        isBodePlot = pResultDef->independentName != NULL;
    }
    else
        isBodePlot = false;
    
    return isBodePlot;    

} /* End of frq_getIsBodePlot */



/**
 * Get the name of the solution: User defined results are identified by name.
 *   @return
 * Get the name the user gave to his user defined result as a read-only string. In case of
 * an "all dependents" result an unspecific, generic name is returned.\n
 *   The returned character pointer points into the solution object and is valid as long as
 * the solution object itself.
 *   @param pSolution
 * The pointer to the solution object.
 */

static inline const char *frq_getResultName(const frq_freqDomainSolution_t *const pSolution)
{
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
                                                      
        /* Here, we can make the distinction between a full result and a Bode plot. */
        name = pResultDef->name;
    }
    else
        name = "AllDependents";
    
    assert(name != NULL  &&  *name != '\0');
    return name;    

} /* End of frq_getResultName */



/**
 * Get the number of knowns of the LES, which the solution belongs to.
 *   @return
 * The number of knowns.
 *   @param pSolution
 * The pointer to the solution object.
 */

static inline unsigned int frq_getNoIndependents
                                (const frq_freqDomainSolution_t *const pSolution)
{
    unsigned int noIndependents;
    
    const tbv_tableOfVariables_t * const pTableOfVars = pSolution->pTableOfVars;
    if(pSolution->idxResult >= 0)
    {
        /* Normal situation: This object is related to a user defined result with a sub-set
           of unknowns and/or user defined voltages as dependents. */
        assert((unsigned)pSolution->idxResult < pTableOfVars->pCircuitNetList->noResultDefs);
        const pci_resultDef_t * const pResultDef = &pTableOfVars
                                                    ->pCircuitNetList
                                                      ->resultDefAry[pSolution->idxResult];
                                                      
        /* We need to make the distinction between a full result and a Bode plot. */
        if(pResultDef->independentName == NULL)
        {
            /* A full result: The independents of this solution are the knowns of the LES. */
            noIndependents = pTableOfVars->noKnowns;
        }
        else
        {
            /* A Bode plot with one explicitly named dependent and independent. */
            assert(pResultDef->noDependents == 1);
            noIndependents = 1;
        }
    }
    else
    {
        /* This object holds a solution for all unknowns of the LES. The independents are the
           unknowns of the LES. */
        noIndependents = pTableOfVars->noKnowns;
    }
    
    return noIndependents;
    
} /* End of frq_getNoIndependents */




/**
 * Get the total number of distinct solutions for distinct dependent quantities, which are
 * available in * \a pSolution. This may include original unknowns from the LES, which the
 * solution belongs to and/or user-defined voltages. In the specific case of a Bode plot
 * result the (only) dependent quantity could even be a known of the LES; here; inverse
 * result definitions are permitted.
 *   @return
 * The number of dependent quantities a solution is available for.
 *   @param pSolution
 * The pointer to the solution object.
 */

static inline unsigned int frq_getNoDependents
                                (const frq_freqDomainSolution_t * const pSolution)
{
    const tbv_tableOfVariables_t * const pTableOfVars = pSolution->pTableOfVars;
    if(pSolution->idxResult >= 0)
    {
        const pci_resultDef_t * const pResultDef = &pTableOfVars
                                                    ->pCircuitNetList
                                                      ->resultDefAry[pSolution->idxResult];
        return pResultDef->noDependents;
    }
    else
        return pTableOfVars->noUnknowns + pTableOfVars->pCircuitNetList->noVoltageDefs;

} /* End of frq_getNoDependents */



/*
 * Global prototypes
 */



#endif  /* FRQ_FREQDOMAINSOLUTION_INLINEINTERFACE_INCLUDED */
