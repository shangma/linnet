/**
 * @file tbv_tableOfVariables.c
 *   This module implements a data structure that holds all symbolic elements of the LES
 * and the solution: The known and unknown variables and the (physical) constants. The
 * table lists these elements together with some of its properties. Access functions are
 * provided that permit to look for a symbolic element by a property.
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
 *   tbv_initModule
 *   tbv_shutdownModule
 *   tbv_setGlobalLogger
 *   tbv_createTableOfVariables
 *   tbv_cloneByShallowCopy
 *   tbv_cloneByReference
 *   tbv_cloneByConstReference
 *   tbv_deleteTableOfVariables
 *   tbv_logTableOfVariables
 *   tbv_addKnown
 *   tbv_addUnknown
 *   tbv_addConstant
 *   tbv_sortConstants
 *   tbv_exportAsMCode
 *   tbv_getKnownByDevice
 *   tbv_getUnknownByNode
 *   tbv_getUnknownByDevice
 *   tbv_getConstantByDevice
 *   tbv_getDeviceByBitIndex
 *   tbv_getReferencedDeviceByBitIndex
 *   tbv_setTargetUnknownForSolver
 * Local functions
 *   checkName
 *   cmpDeviceConstantNames
 */


/*
 * Include files
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "smalloc.h"
#include "stricmp.h"
#include "log_logger.h"
#include "qsort_c.h"
#include "rat_rationalNumber.h"
#include "pci_parserCircuit.h"
#include "msc_mScript.h"
#include "tbv_tableOfVariables.h"


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

#ifdef DEBUG
/** A global counter of all refernces to any created objects. Used to detect memory leaks. */
static unsigned int _noRefsToObjects = 0;
#endif


/*
 * Function implementation
 */

/**
 * Double-check that the name of a known or unknown is not yet in use for another known,
 * unknown or constant. Report a problem in the application log.
 *   @return
 * True if passed name is still unused.
 *   @param pTable
 * A pointer to the table of variables. The object under test has not yet been entered at
 * all.
 *   @param nameToCheck
 * The name to validate. It must not be NULL or the empty string.
 *   @param isKnown
 * Pass \a true if the name of a known quantity is checked and \a false otherwise.
 */

static boolean checkName( tbv_tableOfVariables_t * const pTable
                        , const char * const nameToCheck
                        , boolean isKnown
                        )
{
    assert(nameToCheck != NULL  &&  strlen(nameToCheck) != 0);

    boolean success = true;
    const char *kindOfOtherObj;
    unsigned int u;

    for(u=0; u<pTable->noKnowns; ++u)
    {
        if(strcmp(pTable->knownLookUpAry[u].name, nameToCheck) == 0)
        {
            kindOfOtherObj = "a constant source";
            success = false;
        }
    }
    for(u=0; success && u<pTable->noUnknowns; ++u)
    {
        if(strcmp(pTable->unknownLookUpAry[u].name, nameToCheck) == 0)
        {
            if(pTable->unknownLookUpAry[u].idxNode != PCI_NULL_NODE)
                kindOfOtherObj = "a node's voltage potential";
            else
            {
                assert(pTable->unknownLookUpAry[u].idxDevice != PCI_NULL_DEVICE);
                kindOfOtherObj = "an internal unknown current";
            }
            success = false;
        }
    }
    for(u=0; success && u<pTable->pCircuitNetList->noDevices; ++u)
    {
        /* The names of constant sources are used as known quantities in the LES. The
           device has thus the same name as the known. Consequently, constant sources need
           to be taken out of consideration here.
             For unknown currents: If they belong to current probes then it's permitted
           that the name of the probe is identical to the name of the current.
             These exceptions make it worth a consideration, what the added value of this
           check is; it's not a safe complete check for name ambiguities. For example, what
           about two constant sources having the same name?
             Parser (r584): Constant voltage sources of same name are recognized by the
           parser, same device names also. An op-amp can have the name of a user-defined
           voltage without any warning, error message or recognizable problem (nonetheless
           undesired). 
             Do we need to invent a global name pool, beginning in the parser? */
        if(strcmp(pTable->pCircuitNetList->pDeviceAry[u]->name, nameToCheck) == 0
           && (!isKnown
               || (pTable->pCircuitNetList->pDeviceAry[u]->type != pci_devType_srcU
                   &&  pTable->pCircuitNetList->pDeviceAry[u]->type != pci_devType_srcI
                  )
              )
           && (isKnown
               ||  pTable->pCircuitNetList->pDeviceAry[u]->type != pci_devType_currentProbe
              )
          )
        {
            kindOfOtherObj = "a device";
            success = false;
        }
    }
    
    /* In general, s should be avoided as a name as it will appear in the final result as
       frequency variable. */
    if(success && stricmp("s", nameToCheck) == 0)
    {
        kindOfOtherObj = "the frequency variable";
        success = false;
    }

    if(!success)
    {
        /* Basically a parser message about doubly used name and including a line number
           would be better. But although the parser sorts most of the doubly used names
           indeed out, the message here is not superflous because of the internally
           generated names for the unknowns. */
        LOG_ERROR( _log
                 , "The name %s is ambiguous. It is already in use for %s"
                 , nameToCheck
                 , kindOfOtherObj
                 );
    }

    return success;

} /* End of checkName */




/**
 * Compare function for qsort; used to sort the device constants so that the result
 * presents them in the order R, L, C. Used by \a tbv_sortConstants.
 *   @return
 * >0 if second operand comes prior to the first operand in the aimed sort order\n
 * =0 if if order of bothe opearnds doesn't care in the aimed sort order (they are
 * identical with respect to the sort criteria)\n
 * <1 if second operand comes after the first operand in the aimed sort order
 *   @param pIdxDev1
 * Pointer into the sorted array; points to first comparsion operand.
 *   @param pIdxDev2
 * Pointer into the sorted array; points to second comparsion operand.
 *   @param pContext
 * Context pointer, which is passed unchanged to all invokations of this routine. Here it
 * points to the table of variables.
 */

static signed int cmpDeviceConstantNames( const void *pIdxDev1
                                        , const void *pIdxDev2
                                        , const void *pContext
                                        )
{
    const tbv_tableOfVariables_t * const pTabOfVars = (const tbv_tableOfVariables_t *)pContext;
    const unsigned int idxDev1 = *(unsigned int*)pIdxDev1
                     , idxDev2 = *(unsigned int*)pIdxDev2;
    assert(pTabOfVars != NULL  &&  idxDev1 != idxDev2
           &&  idxDev1 < pTabOfVars->pCircuitNetList->noDevices
           &&  idxDev2 < pTabOfVars->pCircuitNetList->noDevices
          );
    const pci_device_t * const * const pDeviceAry = pTabOfVars->pCircuitNetList->pDeviceAry;

    /* The constants are mainly sorted according to their type, R before L before C. (The
       actual order is determined by the order of the enumeration, which is used as numeric
       type here.) */
    if((int)pDeviceAry[idxDev1]->type != (int)pDeviceAry[idxDev2]->type)
        return (int)pDeviceAry[idxDev2]->type - (int)pDeviceAry[idxDev1]->type;
    else
    {
        /* The sub-ordinated sort criterion is the name in alphabetic order. This will
           yield the expected behavior for the common use case of having names like R1, R2,
           R3. However, R10 would come before R9. */
        return stricmp(pDeviceAry[idxDev2]->name, pDeviceAry[idxDev1]->name);
    }
} /* End of cmpDeviceConstantNames */




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
 *   @see void tbv_shutdownModule()
 */

void tbv_initModule(log_hLogger_t hLogger)
{
    assert(hLogger != LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT);
    _log = log_cloneByReference(hLogger);

#ifdef  DEBUG
    /* The DEBUG compilation counts all references to all created objects. */
    _noRefsToObjects = 0;
#endif
} /* End of tbv_initModule */




/**
 * Do all cleanup after use of the module, which is required to avoid memory leaks, orphaned
 * handles, etc.
 */

void tbv_shutdownModule()
{
#ifdef  DEBUG
    /* The DEBUG compilation looks for still allocated objects in order to detect memory
       leaks. */
    if(_noRefsToObjects != 0)
    {
        fprintf( stderr
               , "tbv_shutdownModule: %u references to objects of type tbv_tableOfVariables_t"
                 " have not been discarded at application shutdown. There are probable"
                 " memory leaks\n"
               , _noRefsToObjects
               );
    }
#endif

    /* Discard reference to the logger object. */
    log_deleteLogger(_log);

} /* End of tbv_shutdownModule */




/**
 * Create a table of variables, which is still empty. Use the set of tbv_add functions to
 * fill the table.
 *   @return
 * A pointer to the new table is returned. Some memory has been allocated on the heap;
 * therefore the object needs to be deleted again after usage.
 *   @param noKnowns
 * The number of knowns to store in the table. This number needs to be precisely known at
 * time of creation: Adding elements is only double-checked by assertion in the DEBUG
 * compilation.
 *   @param noUnknowns
 * The number of unknowns to store in the table. This number needs to be precisely known at
 * time of creation: Adding elements is only double-checked by assertion in the DEBUG
 * compilation.
 *   @param noConstants
 * The number of (physical) constants to store in the table. This number needs to be
 * precisely known at time of creation: Adding elements is only double-checked by assertion
 * in the DEBUG compilation.
 *   @param pCircuitNetList
 * The elements of a table of variables refer to a parse result. Pass the pointer or
 * reference to the related parse result object.
 *   @remark
 * After creation the table will be filled using the different tbv_add functions. The table
 * will then contain references into the parse result, another data structure. The client
 * has to ensure that a copy (by reference) of this other data structure is held in this
 * object. See tbv_setReferenceToNetList for more.
 *   @see void tbv_deleteTableOfVariables(tbv_tableOfVariables_t * const)
 *   @see const pci_circuit_t *tbv_setReferenceToNetList(tbv_tableOfVariables_t *
 * const, const pci_circuit_t * const)
 */

tbv_tableOfVariables_t *tbv_createTableOfVariables( unsigned int noKnowns
                                                  , unsigned int noUnknowns
                                                  , unsigned int noConstants
                                                  , const pci_circuit_t * const pCircuitNetList
                                                  )
{
    assert(_log != LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT  &&  pCircuitNetList != NULL);
    assert(noConstants <= pCircuitNetList->noDevices
           &&  noKnowns <= pCircuitNetList->noDevices
           &&  noUnknowns < pCircuitNetList->noNodes + pCircuitNetList->noDevices
          );

    tbv_tableOfVariables_t *pTab = smalloc( sizeof(tbv_tableOfVariables_t)
                                          , __FILE__
                                          , __LINE__
                                          );

    /* The result of the constructor is the first reference to the new object. */
    pTab->noReferencesToThis = 1;

    pTab->maxNoKnowns = noKnowns;
    pTab->noKnowns = 0;
    pTab->knownLookUpAry = smalloc(sizeof(*pTab->knownLookUpAry)*noKnowns, __FILE__, __LINE__);

    pTab->maxNoUnknowns = noUnknowns;
    pTab->noUnknowns = 0;
    pTab->unknownLookUpAry = smalloc( sizeof(*pTab->unknownLookUpAry)*noUnknowns
                                    , __FILE__
                                    , __LINE__
                                    );
    pTab->nodeIdxToUnknownIdxAry = smalloc( sizeof(*pTab->nodeIdxToUnknownIdxAry)
                                            * pCircuitNetList->noNodes
                                          , __FILE__
                                          , __LINE__
                                          );
    pTab->devIdxToKnownIdxAry = smalloc( sizeof(*pTab->devIdxToKnownIdxAry)
                                         * pCircuitNetList->noDevices
                                       , __FILE__
                                       , __LINE__
                                       );
    pTab->devIdxToUnknownIdxAry = smalloc( sizeof(*pTab->devIdxToUnknownIdxAry)
                                           * pCircuitNetList->noDevices
                                         , __FILE__
                                         , __LINE__
                                         );

    /* The mapping tables that relate node or device indexes to unknowns needs to be
       initialized: There are less unknowns than nodes and devices and function addUnknown
       won't fill all elements of these tables. The not used entries get the resevered
       value UINT_MAX, which means this node/device has no related unknown. */
    unsigned int u;
    for(u=0; u<pCircuitNetList->noNodes; ++u)
        pTab->nodeIdxToUnknownIdxAry[u] = UINT_MAX;
    for(u=0; u<pCircuitNetList->noDevices; ++u)
    {
        pTab->devIdxToKnownIdxAry[u] = UINT_MAX;
        pTab->devIdxToUnknownIdxAry[u] = UINT_MAX;
    }

    pTab->maxNoConstants = noConstants;
    pTab->noConstants = 0;
    pTab->constantIdxToDevIdxAry = smalloc( sizeof(*pTab->constantIdxToDevIdxAry)
                                            * pTab->maxNoConstants
                                          , __FILE__
                                          , __LINE__
                                          );
    pTab->devIdxToConstantIdxAry = smalloc( sizeof(*pTab->devIdxToConstantIdxAry)
                                            * pCircuitNetList->noDevices
                                          , __FILE__
                                          , __LINE__
                                          );
    for(u=0; u<pTab->maxNoConstants; ++u)
        pTab->constantIdxToDevIdxAry[u] = UINT_MAX;
    for(u=0; u<pCircuitNetList->noDevices; ++u)
        pTab->devIdxToConstantIdxAry[u] = UINT_MAX;

    pTab->pCircuitNetList = pci_cloneByConstReference(pCircuitNetList);

#ifdef DEBUG
    ++ _noRefsToObjects;
#endif

    return pTab;

} /* End of tbv_createTableOfVariables */





/**
 * Copy a table of variables. It is a shallow copy, the immediate members are copied, in
 * particular the arrays of knowns, unknowns and constants, but the associated net list
 * object is copied only by reference.
 *   @return
 * A pointer to a new table is returned. Some memory has been allocated on the heap;
 * therefore the object needs to be deleted again after usage.
 *   @param pExistingObj
 * The number of knowns to store in the table. This number needs to be precisely known at
 * time of creation: Adding elements is only double-checked by assertion in the DEBUG
 * compilation.object to copy is passed by reference.
 *   @see void tbv_deleteTableOfVariables(tbv_tableOfVariables_t * const)
 */

tbv_tableOfVariables_t *tbv_cloneByShallowCopy
                            (const tbv_tableOfVariables_t * const pExistingObj)

{
    tbv_tableOfVariables_t *pCopy = smalloc( sizeof(tbv_tableOfVariables_t)
                                           , __FILE__
                                           , __LINE__
                                           );

    /* The result of the copy operation is the first reference to this new object. */
    pCopy->noReferencesToThis = 1;

    /* Although the operation as such is well defined, it's useless to make a copy of a
       still uncompleted table of variables. This is checked by asssertion.
         This check is not a self-check of the class tbv but a test of the intended client
       code! */
    assert(pExistingObj->noKnowns == pExistingObj->maxNoKnowns
           &&  pExistingObj->noUnknowns == pExistingObj->maxNoUnknowns
           &&  pExistingObj->noConstants == pExistingObj->maxNoConstants
          );

    unsigned int i, noEntries;

    noEntries = pExistingObj->noKnowns;
    pCopy->maxNoKnowns = pExistingObj->maxNoKnowns;
    pCopy->noKnowns = noEntries;
    pCopy->knownLookUpAry = smalloc( sizeof(*pCopy->knownLookUpAry)*pCopy->maxNoKnowns
                                   , __FILE__
                                   , __LINE__
                                   );
    for(i=0; i<noEntries; ++i)
    {
        pCopy->knownLookUpAry[i].name = stralloccpy(pExistingObj->knownLookUpAry[i].name);
        pCopy->knownLookUpAry[i].idxCol = pExistingObj->knownLookUpAry[i].idxCol;
    }

    noEntries = pExistingObj->noUnknowns;
    pCopy->maxNoUnknowns = pExistingObj->maxNoUnknowns;
    pCopy->noUnknowns = noEntries;
    pCopy->unknownLookUpAry = smalloc( sizeof(*pCopy->unknownLookUpAry)*pCopy->maxNoUnknowns
                                     , __FILE__
                                     , __LINE__
                                     );
    for(i=0; i<noEntries; ++i)
    {
        pCopy->unknownLookUpAry[i].name = stralloccpy(pExistingObj->unknownLookUpAry[i].name);
        pCopy->unknownLookUpAry[i].idxNode = pExistingObj->unknownLookUpAry[i].idxNode;
        pCopy->unknownLookUpAry[i].idxRow = pExistingObj->unknownLookUpAry[i].idxRow;
        pCopy->unknownLookUpAry[i].idxCol = pExistingObj->unknownLookUpAry[i].idxCol;
        pCopy->unknownLookUpAry[i].idSubNet = pExistingObj->unknownLookUpAry[i].idSubNet;
    }

    noEntries = pExistingObj->pCircuitNetList->noNodes;
    pCopy->nodeIdxToUnknownIdxAry = smalloc( sizeof(*pCopy->nodeIdxToUnknownIdxAry)
                                             * noEntries
                                           , __FILE__
                                           , __LINE__
                                           );
    for(i=0; i<noEntries; ++i)
        pCopy->nodeIdxToUnknownIdxAry[i] = pExistingObj->nodeIdxToUnknownIdxAry[i];

    noEntries = pExistingObj->pCircuitNetList->noDevices;
    pCopy->devIdxToKnownIdxAry = smalloc( sizeof(*pCopy->devIdxToKnownIdxAry) * noEntries
                                        , __FILE__
                                        , __LINE__
                                        );
    for(i=0; i<noEntries; ++i)
        pCopy->devIdxToKnownIdxAry[i] = pExistingObj->devIdxToKnownIdxAry[i];

    noEntries = pExistingObj->pCircuitNetList->noDevices;
    pCopy->devIdxToUnknownIdxAry = smalloc( sizeof(*pCopy->devIdxToUnknownIdxAry) * noEntries
                                          , __FILE__
                                          , __LINE__
                                          );
    for(i=0; i<noEntries; ++i)
        pCopy->devIdxToUnknownIdxAry[i] = pExistingObj->devIdxToUnknownIdxAry[i];

    noEntries = pExistingObj->noConstants;
    pCopy->maxNoConstants = pExistingObj->maxNoConstants;
    pCopy->noConstants = noEntries;
    pCopy->constantIdxToDevIdxAry = smalloc( sizeof(*pCopy->constantIdxToDevIdxAry)
                                             * pCopy->maxNoConstants
                                           , __FILE__
                                           , __LINE__
                                           );
    for(i=0; i<noEntries; ++i)
        pCopy->constantIdxToDevIdxAry[i] = pExistingObj->constantIdxToDevIdxAry[i];

    noEntries = pExistingObj->pCircuitNetList->noDevices;
    pCopy->devIdxToConstantIdxAry = smalloc( sizeof(*pCopy->devIdxToConstantIdxAry) * noEntries
                                           , __FILE__
                                           , __LINE__
                                           );
    for(i=0; i<noEntries; ++i)
        pCopy->devIdxToConstantIdxAry[i] = pExistingObj->devIdxToConstantIdxAry[i];

    /* The anyway never changed object holding the parse result can be copied by reference. */
    pCopy->pCircuitNetList = pci_cloneByConstReference(pExistingObj->pCircuitNetList);

#ifdef DEBUG
    ++ _noRefsToObjects;
#endif

    return pCopy;

} /* End of tbv_cloneByShallowCopy */





/**
 * Request a reference to a table of variables. The new reference is counted internally for
 * later and safe control of the delete operation.\n
 *   Any requested reference needs to be freed with tbv_deleteTableOfVariables after use.
 *   @return
 * A copy of the passed pointer \a pTabOfVars is returned.
 *   @param pTabOfVars
 * Pointer to the object to be cloned by reference.
 *   @see tbv_tableOfVariables_t *tbv_createTableOfVariables(unsigned int, unsigned int,
 * unsigned int)
 *   @see void tbv_deleteTableOfVariables(tbv_tableOfVariables_t * const)
 */

tbv_tableOfVariables_t *tbv_cloneByReference(tbv_tableOfVariables_t * const pTabOfVars)
{
    assert(pTabOfVars != NULL);
    ++ pTabOfVars->noReferencesToThis;

#ifdef DEBUG
    ++ _noRefsToObjects;
#endif

    return pTabOfVars;

} /* End of tbv_cloneByReference */





/**
 * Request a reference to a constant table of variables. The new reference is counted
 * internally for later and safe control of the delete operation.\n
 *   Any requested reference needs to be freed with tbv_deleteTableOfVariables after use.
 *   @return
 * A read-only copy of the passed pointer \a pTabOfVars is returned.
 *   @param pTabOfVars
 * Pointer to the (read-only) object to be cloned by reference.
 *   @see tbv_tableOfVariables_t *tbv_createTableOfVariables(unsigned int, unsigned int,
 * unsigned int)
 *   @see void tbv_deleteTableOfVariables(tbv_tableOfVariables_t * const)
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

const tbv_tableOfVariables_t *tbv_cloneByConstReference
                                            (const tbv_tableOfVariables_t * const pTabOfVars)
{
    return tbv_cloneByReference((tbv_tableOfVariables_t*)pTabOfVars);

} /* End of tbv_cloneByConstReference */





/**
 * Delete a reference to a table of variables after use. If there are no references left
 * then the object itself is deleted, all memory is freed.
 *   @param pConstTabOfVars
 * Pointer to the object to be deleted. No action if this is the NULL pointer.\n
 *   @see tbv_tableOfVariables_t *tbv_createTableOfVariables(unsigned int, unsigned int,
 * unsigned int)
 *   @see tbv_tableOfVariables_t *tbv_cloneByReference(tbv_tableOfVariables_t *)
 *   @remark
 * Regardless of the destructive operation the object is declared read-only in order to
 * support deletion of read-only copies of references to existing objects.
 */

void tbv_deleteTableOfVariables(const tbv_tableOfVariables_t * const pConstTabOfVars)
{
    if(pConstTabOfVars == NULL)
        return;

    /* See discussion of const declaration at tbv_cloneByConstReference. The idea of const
       objects requires that we hurt the type qualifier locally here. */
    tbv_tableOfVariables_t * const pTabOfVars = (tbv_tableOfVariables_t*)pConstTabOfVars;

    /* Deletion takes place only if there are no known other references to this object. */
    assert(pTabOfVars->noReferencesToThis >= 1);
    if(--pTabOfVars->noReferencesToThis == 0)
    {
        /* Delete the reference to the related parse result. */
        pci_deleteParseResult(pTabOfVars->pCircuitNetList);

        unsigned int u;

        for(u=0; u<pTabOfVars->noKnowns; ++u)
            free((void*)pTabOfVars->knownLookUpAry[u].name);
        free(pTabOfVars->knownLookUpAry);

        for(u=0; u<pTabOfVars->noUnknowns; ++u)
            free((void*)pTabOfVars->unknownLookUpAry[u].name);
        free(pTabOfVars->unknownLookUpAry);
        free(pTabOfVars->nodeIdxToUnknownIdxAry);
        free(pTabOfVars->devIdxToKnownIdxAry);
        free(pTabOfVars->devIdxToUnknownIdxAry);

        free(pTabOfVars->constantIdxToDevIdxAry);
        free(pTabOfVars->devIdxToConstantIdxAry);

        free(pTabOfVars);
    }

#ifdef DEBUG
    -- _noRefsToObjects;
#endif
} /* End of tbv_deleteTableOfVariables */





/**
 * Diagnostic function: Write the contents of a table of variables object to the
 * application log. Logging is done on level DEBUG.
 *   @param pTable
 * A reference to the table to be printed.
 */

void tbv_logTableOfVariables(const tbv_tableOfVariables_t * const pTable)
{
    assert(pTable != NULL  &&  _log != LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT);

    /* All logging is done on level DEBUG; we can take a shortcut if this verbosity is not
       desired. */
    if(!log_checkLogLevel(_log, log_debug))
        return;

    unsigned int u;

    LOG_DEBUG(_log, "Table of symbolic objects:");

    LOG_DEBUG(_log, "%u known variables:", pTable->noKnowns);
    for(u=0; u<pTable->noKnowns; ++u)
    {
        LOG_DEBUG( _log
                 , "  %u) %s, column in LES: %u"
                 , u
                 , pTable->knownLookUpAry[u].name
                 , pTable->knownLookUpAry[u].idxCol
                 );
    }

    LOG_DEBUG(_log, "%u unknown variables:", pTable->noUnknowns);
    for(u=0; u<pTable->noUnknowns; ++u)
    {
        const char * const kindOfUnknown = pTable->unknownLookUpAry[u].idxNode != PCI_NULL_NODE
                                           ? "voltage of node"
                                           : "internal unknown current";
        LOG_DEBUG( _log
                 , "  %u) %s, %s, position in LES: (%u, %u), sub-net: %d"
                 , u
                 , pTable->unknownLookUpAry[u].name
                 , kindOfUnknown
                 , pTable->unknownLookUpAry[u].idxRow
                 , pTable->unknownLookUpAry[u].idxCol
                 , pTable->unknownLookUpAry[u].idxNode != PCI_NULL_NODE
                   ? (int)pTable->unknownLookUpAry[u].idSubNet
                   : -1
                 );
    }

    LOG_DEBUG(_log, "%u constants:", pTable->noConstants);
    coe_productOfConst_t constant = 0x01;
    for(u=0; u<pTable->noConstants; ++u, constant<<=1)
    {
        const pci_device_t * const pDev = tbv_getDeviceByBitIndex(pTable, u);

        /* Portable code: GCC on MinGW doesn't support the printf formatting character %llx
           but requires %I64x, see
           http://stackoverflow.com/questions/13590735/printf-long-long-int-in-c-with-gcc,
           Feb 2014. */
#if defined(__WIN32) || defined(__WIN64)
# define F64X    "%I64x"
#else
# define F64X    "%llx"
#endif
        LOG_DEBUG( _log
                 , "  %u) %s, %s, internal representation: 0x" F64X
                 , u
                 , pDev->name
                 , pci_getNameOfDeviceType(pDev)
                 , (long long unsigned)constant
                 );
#undef F64X
    }
} /* End of tbv_logTableOfVariables */




/**
 * Add a known variable to the table and assign a column of the LES, which holds the
 * coefficients related to this known.
 *   @return
 * Success. Failures can be ambiguous names, an overful table or an invalid column index.
 *   @param pTable
 * A pointer to the table of variables, which the known is to be put into.
 *   @param name
 * The name of the known. Mainly used for reporting results. The string is copied into
 * local memory of the table object. The passed pointer needs to be valid only during
 * function execution. The name must not be NULL or the empty string.
 *   @param idxDevice
 * Knowns are related to devices (constant sources). A lookup function by device index is
 * defined. To support the lookup function the index of the device is passed to this
 * function. The index is related to the linear array of devices in the parse result (i.e.
 * the circuit net list object).
 *   @remark
 * An error because of an overful table is reported by assertion only as it is considered
 * an internal implementation error. Errors caused by bad user input are reported to the
 * application log.
 */

boolean tbv_addKnown( tbv_tableOfVariables_t * const pTable
                    , const char * const name
                    , unsigned int idxDevice
                    )
{
    assert(pTable != NULL  &&  _log != LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT);

    /* Doubly defined names are considered an error: Any reported result would become
       meaningless. This is a user input caused problem, which is reported in the
       application log. */
    boolean success = checkName(pTable, name, /* isKnown */ true);

    /* Try to place the known in the next free array entry. */
    if(pTable->noKnowns >= pTable->maxNoKnowns)
    {
        assert(false);
        success = false;
    }

    if(success)
    {
        unsigned int idx = pTable->noKnowns++;
        pTable->knownLookUpAry[idx].name = stralloccpy(name);

        /* Use next index value for the column of the LES. */
        if(idx == 0)
        {
            /* The right most columns of the LES are reserved for the knowns. The left
               columns hold the coefficients of the unknowns. */
            pTable->knownLookUpAry[idx].idxCol = pTable->maxNoUnknowns;
        }
        else
            pTable->knownLookUpAry[idx].idxCol = pTable->knownLookUpAry[idx-1].idxCol + 1;

        /* Update the related mapping table. */
        assert(idxDevice < pTable->pCircuitNetList->noDevices
               &&  pTable->devIdxToKnownIdxAry[idxDevice] == UINT_MAX
              );
        pTable->devIdxToKnownIdxAry[idxDevice] = idx;
    }

    return success;

} /* End of tbv_addKnown */





/**
 * Add an unknown variable to the table and assign a column of the LES, that holds the
 * coefficients related to this unknown.
 *   @return
 * Success. Failures can be ambiguous names, an overful table or an invalid node or device
 * index.
 *   @param pTable
 * A pointer to the half-way completed table of variables, which the unknown is to be put
 * into.
 *   @param name
 * The name of the unknown. Mainly used for reporting results. The string is copied into
 * local memory of the table object. The passed pointer needs to be valid only during
 * function execution. The name must not be NULL or the empty string.
 *   @param idxNode
 * Most unknowns are voltages of nodes of the network. For these a lookup function by node
 * is defined. To support the lookup function the index of the node is passed to this
 * function. The index is related to the linear array of nodes in the parse result.
 * Unknowns, that are not node voltages specify #PCI_NULL_NODE.
 *   @param idSubNet
 * If idxNode is not #PCI_NULL_NODE then the ID of the sub-graph of the complete circuit is
 * passed. All sub-graphs have a unique ID. The value UINT_MAX is reserved and must not be
 * used as ID. Instead, pass UINT_MAX if the unknown is related to a device rather than to
 * a node's voltage potential.
 *   @param idxDevice
 * Unknowns, which are not voltages of nodes of the network are related to devices (device
 * currents mostly). For these a lookup function by device index is defined. To support the
 * lookup function the index of the device is passed to this function. The index is related
 * to the linear array of devices in the parse result. Unknowns, that are not device
 * related specify #PCI_NULL_DEVICE.\n
 *   Either \a idxNode or \a idxDevice is specified but not both at a time.
 *   @remark
 * An error because of an overful table is reported by assertion only as it is considered
 * an internal implementation error. Errors caused by bad user input are reported to the
 * application log.
 */

boolean tbv_addUnknown( tbv_tableOfVariables_t * const pTable
                      , const char * const name
                      , unsigned int idxNode
                      , unsigned int idSubNet
                      , unsigned int idxDevice
                      )
{
    assert(pTable != NULL  &&  _log != LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT);
    assert(idxNode == PCI_NULL_NODE  ||  idxDevice == PCI_NULL_DEVICE);

    /* Doubly defined names are considered an error: Any reported result would become
       meaningless. This is a user input caused problem, which is reported in the
       application log. */
    boolean success = checkName(pTable, name, /* isKnown */ false);
    if(!success)
    {
        LOG_ERROR( _log
                 , "%s is the name of an unknown quantity. The name is internally derived from"
                   " a node or device name by a simple process, which neither recognizes"
                   " nor resolves clashes with existing names. Please rename the object"
                   " with the conflicting name instead"
                 , name
                 );
    }
    
    /* Try to place the unknown in the next free array entry. */
    if(pTable->noUnknowns >= pTable->maxNoUnknowns)
    {
        assert(false);
        success = false;
    }

    if(success)
    {
        unsigned int idx = pTable->noUnknowns++;
        pTable->unknownLookUpAry[idx].name = stralloccpy(name);

        assert(idxNode == PCI_NULL_NODE  ||  idxNode < pTable->pCircuitNetList->noNodes);
        pTable->unknownLookUpAry[idx].idxNode = idxNode;

        assert(idxDevice == PCI_NULL_DEVICE || idxDevice < pTable->pCircuitNetList->noDevices);
        pTable->unknownLookUpAry[idx].idxDevice = idxDevice;

        /* Use next index value for the row and column of the LES. */
        if(idx == 0)
        {
            pTable->unknownLookUpAry[idx].idxRow =
            pTable->unknownLookUpAry[idx].idxCol = 0;
        }
        else
        {
            pTable->unknownLookUpAry[idx].idxRow =
            pTable->unknownLookUpAry[idx].idxCol = pTable->unknownLookUpAry[idx-1].idxCol + 1;
        }

        assert((idSubNet != UINT_MAX  &&  idxNode != PCI_NULL_NODE)
               ||  (idSubNet == UINT_MAX  &&  idxDevice != PCI_NULL_DEVICE)
              );
        pTable->unknownLookUpAry[idx].idSubNet = idSubNet;

        /* Update the related mapping table. */
        if(idxNode < pTable->pCircuitNetList->noNodes)
        {
            assert(pTable->nodeIdxToUnknownIdxAry[idxNode] == UINT_MAX);
            pTable->nodeIdxToUnknownIdxAry[idxNode] = idx;
        }
        else
        {
            assert(idxDevice < pTable->pCircuitNetList->noDevices
                   &&  pTable->devIdxToUnknownIdxAry[idxDevice] == UINT_MAX
                  );
            pTable->devIdxToUnknownIdxAry[idxDevice] = idx;
        }
    }

    return success;

} /* End of tbv_addUnknown */





/**
 * Add a physical device constant to the table.
 *   @param pTable
 * A pointer to the table of variables, which the constant is to be put into.
 *   @param idxDevice
 * A device's constant is closely related to the device. The table therefore holds the
 * reference to a device object instead of a dedicated object to represent the constant.\n
 *   The reference is implemented as the index of the device in the array of devices inside
 * the parse result. (A read-only copy of the parse result is found inside the object * \a
 * pTable.) The index refers to this table.
 *   @remark
 * An error because of an overful table is reported by assertion only as it is considered
 * an internal implementation error.
 */

void tbv_addConstant(tbv_tableOfVariables_t * const pTable, unsigned int idxDevice)
{
    assert(pTable != NULL  &&  _log != LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT
           &&  idxDevice < pTable->pCircuitNetList->noDevices
          );

    /* The constant gets the next free index. */
    unsigned int idx = pTable->noConstants++;
    assert(pTable->noConstants <= pTable->maxNoConstants);

    /* Store device lookup index and inverse lookup. */
    assert(pTable->constantIdxToDevIdxAry[idx] == UINT_MAX
           &&  pTable->devIdxToConstantIdxAry[idxDevice] == UINT_MAX
          );
    pTable->constantIdxToDevIdxAry[idx] = idxDevice;
    pTable->devIdxToConstantIdxAry[idxDevice] = idx;

} /* End of tbv_addConstant */





/**
 * After having added all device constants with \a tbv_addConstant but prior to using the
 * shaped association between a constant's name and its internal representation as a \a
 * coe_productOfConst_t the very first time, the order of the constants in the table may be
 * chosen. With other words: Which constant is associated with bit 0 of the internal
 * representation, which one with bit 1, etc. The order gets apparent in all result output,
 * where the constants with higher bit index are printed first. The most natural order of
 * output of products of constants is R(esistor) before L (inductivity) before C(apacitor).
 * This routine shapes an according order.
 *   @param pTable
 * A pointer to the object, whose constants are to be sorted.
 *   @remark
 * \a tbv_addConstant should not be called after calling this method; \a
 * tbv_getConstantByDevice, \a tbv_getDeviceByBitIndex and \a
 * tbv_getReferencedDeviceByBitIndex must not be called before calling this method;
 *   @see void tbv_addConstant(tbv_tableOfVariables_t * const, unsigned int)
 */

void tbv_sortConstants(tbv_tableOfVariables_t * const pTable)
{
    /* Sort the indexes to the devices, so that the devices' names appear in the expected
       order if we iterate through the index array. This is ensured by the compare
       function, which does a name lookup from the indexes to copmpare passed to it. */
    qsort_c( pTable->constantIdxToDevIdxAry
           , pTable->noConstants
           , sizeof(pTable->constantIdxToDevIdxAry[0])
           , cmpDeviceConstantNames
           , pTable
           );

    /* qsort won't sort the inverse lookup table. We have to completely rewrite it. */
    unsigned int idxConstant;
    for(idxConstant=0; idxConstant<pTable->noConstants; ++idxConstant)
    {
        unsigned int idxDevice = pTable->constantIdxToDevIdxAry[idxConstant];
        pTable->devIdxToConstantIdxAry[idxDevice] = idxConstant;
    }
} /* End of tbv_sortConstants */




/**
 * Write the elements that are relevant to the executable Octave code into an M script. As
 * a matter of fact, these are only the values of the device constants. To have executable
 * code in any case the method writes default values to the M script where the input file
 * doesn't specify actual values.
 *   @param pTable
 * The reference to the exported table of symbolic objects is passed.
 *   @param pMScript
 * The pointer to an M script object. The generated M code is written into this M script.
 * The object is the result of a successful call of boolean msc_createMScript(msc_mScript_t
 * ** const, const char * const, const char * const, const char * const).
 *   @param context
 * The variables can be exported in different contexts of M code. Choose, which context to
 * support.
 *   @param indentStr
 * The generated code starts with a new line. This and each other required line will begin
 * with the passed string.
 */

void tbv_exportAsMCode( const tbv_tableOfVariables_t * const pTable
                      , msc_mScript_t * const pMScript
                      , tbv_contextOfMCode_t context
                      , const char * const indentStr
                      )
{
    /* In the generated M code the device constants are collected in a parameter struct
       with fixed, given name. */
    const char * const nameOfParameterStruct = "deviceConstants";

    /* Write into the stream associated with the M script object. */
    FILE *hMFile = msc_borrowStream(pMScript);

    const char *titleFmtStr;
    switch(context)
    {
    default:
        assert(false);
    case tbv_assignDefaultValues:
        titleFmtStr = "%s%% The default values of the device constants.\n";
        break;

    case tbv_assignParameterStruct:
        titleFmtStr = "%s%% The values of the device constants are taken from function"
                      " argument %s.\n";
        break;

    case tbv_copyToParameterStruct:
        titleFmtStr = "%s%% The values of the device constants are assigned to function"
                      " result %s.\n";
        break;
    }
    fprintf(hMFile, titleFmtStr, indentStr, nameOfParameterStruct);

    /* Result initialization is needed only in one context: Create an empty struct prior to
       separate assignments of members. */
    if(context == tbv_copyToParameterStruct)
        fprintf(hMFile, "%s%s  \t= struct;\n", indentStr, nameOfParameterStruct);

    /* Write a simple assignment for each constant a numeric value is specified for. The
       inverse iteration through the table is used to get the common order of appearance of
       the constants: R before L before C. */
    const pci_device_t * const * const pDevAry = pTable->pCircuitNetList->pDeviceAry;

    signed int idxC;
    for(idxC=pTable->noConstants-1; idxC>=0; --idxC)
    {
        const unsigned int idxDev = pTable->constantIdxToDevIdxAry[idxC];
        assert(idxDev < pTable->pCircuitNetList->noDevices);
        const pci_device_t * const pDev = pDevAry[idxDev];

        /* Only those device constants are relevant to the M code, which are not related to
           another device. */
        if(pDev->devRelation.idxDeviceRef != PCI_NULL_DEVICE)
            continue;

        /* The generated code snippets look like:
             % The values of the device constants are taken from struct deviceConstants.
             R   = deviceConstants.R;
             Ct1 = deviceConstants.Ct1;
             Ct2 = deviceConstants.Ct2;
             Ct3 = deviceConstants.Ct3;

             % The default values of the device constants.
             R   = 0.159155;
             Ct1 = 0.01;
             Ct2 = 0.0001;
             Ct3 = 1e-006;

             % The values of the device constants are assigned to struct deviceConstants.
             deviceConstants     = struct;
             deviceConstants.R   = R;
             deviceConstants.Ct1 = Ct1;
             deviceConstants.Ct2 = Ct2;
             deviceConstants.Ct3 = Ct3; */

        switch(context)
        {
        case tbv_assignDefaultValues:
        {
            double value = pDev->numValue;
            if(value < 0)
            {
                /* No numeric value has been specified. We take a static default value. */
                assert(pDev->devRelation.idxDeviceRef == PCI_NULL_DEVICE);
                switch(pDev->type)
                {
                    /* The hardcoded default values are taken such that the typical
                       time constants RC, sqrt(LC) and L/R are all in the NF range, at
                       1ms. */
                    case pci_devType_resistor: value = 100.0; break;
                    case pci_devType_conductance: value = 1.0/100.0; break;
                    case pci_devType_inductivity: value = 1e-3; break;
                    case pci_devType_capacitor: value = 10e-6; break;

                    /** @todo Find reasonable default values for controlled voltage
                        sources. What are typical use cases? */
                    case pci_devType_srcUByU:
                    case pci_devType_srcUByI: value = 1.0; break;

                    /* The hard coded default value considers the typical use case of
                       modelling a unipolar transistor, which has a drain-source
                       current modulation of a few milliamperes for a gate-source
                       voltage modulation of a few volts. (A n-channel JFET requires a
                       negative sign, which needs to be considered by the definition of
                       the polarity of the control voltage of the source.) */
                    case pci_devType_srcIByU: value = 0.005; break;

                    /* The hard coded default value considers the typical use case of
                       modelling a bipolar transistor, which has a current
                       amplification of a few hundred. */
                    case pci_devType_srcIByI: value = 250.0; break;

                    /* No result relevant value needed for knowns. */
                    case pci_devType_srcI:
                    case pci_devType_srcU:

                    /* No value is defined for (ideal) op-amps. */
                    case pci_devType_opAmp:

                    /* No value is defined for current probes. */
                    case pci_devType_currentProbe:

                    default: assert(false);
                }

                LOG_INFO( _log
                        , "Device constant %s is assigned the default value %g"
                        , pDev->name
                        , value
                        )

            } /* End if(A numeric value is specified for the device?) */

            assert(value >= 0.0);
            if(value == 0.0)
            {
                LOG_WARN( _log
                        , "The device constant %s has the suspicious value null."
                          " Please check your circuit file"
                        , pDev->name
                        )
                fprintf( hMFile
                       , "%swarning('The device constant %s has the suspicious value"
                         " null. Please check your circuit file')\n"
                       , indentStr
                       , pDev->name
                       );
            }
            fprintf(hMFile, "%s%s\t= %.6g;\n", indentStr, pDev->name, value);
        }
        break;

        case tbv_assignParameterStruct:
        {
            fprintf( hMFile
                   , "%s%s\t= %s.%s;\n"
                   , indentStr
                   , pDev->name
                   , nameOfParameterStruct
                   , pDev->name
                   );
        }
        break;

        case tbv_copyToParameterStruct:
        {
            fprintf( hMFile
                   , "%s%s.%s\t= %s;\n"
                   , indentStr
                   , nameOfParameterStruct
                   , pDev->name
                   , pDev->name
                   );
        }
        break;

        default:
            assert(false);
            break;

        } /* End switch(Which M code context?) */

    } /* End for(All constants) */

    msc_releaseStream(pMScript);

} /* End of tbv_exportAsMCode */




/**
 * Look for a known variable related to a device (a constant source) in the lookup table of
 * knowns.
 *   @return
 * The internal representation of the known is returned by reference. If \a idxDevice is
 * not the index of a device, that defines an known voltage or current in the LES then an
 * assertion fires.
 *   @param pTable
 * The reference to the filled table of symbolic objects is passed.
 *   @param idxDevice
 * The index of the device, the known is related to. The index refers to the array of devices
 * in the parse result. PCI_NULL_DEVICE must not be passed as it is not a unique device
 * identifier.
 */

const tbv_knownVariable_t *tbv_getKnownByDevice
                                    ( const tbv_tableOfVariables_t * const pTable
                                    , unsigned int idxDevice
                                    )
{
    assert(idxDevice < pTable->pCircuitNetList->noDevices);
    const unsigned int idxKnown = pTable->devIdxToKnownIdxAry[idxDevice];
    if(idxKnown != UINT_MAX)
        return &pTable->knownLookUpAry[idxKnown];
    else
    {
        /* This case indicates an error in the client code. */
        assert(false);
        return NULL;
    }
} /* End of tbv_getKnownByDevice */




/**
 * Look for an unknown variable by node in the lookup table of those.
 *   @return
 * The internal representation of the unknown is returned if \a idxNode represents one of
 * the independent network nodes. If it designates a ground node then NULL is returned.
 *   @param pTable
 * The reference to the filled table of symbolic objects is passed.
 *   @param idxNode
 * The index of the node, the unknown is related to. The index refers to the array of nodes
 * in the parse result. PCI_NULL_NODE must not be passed as it is not a unique node
 * identifier.
 */

const tbv_unknownVariable_t *tbv_getUnknownByNode( const tbv_tableOfVariables_t * const pTable
                                                 , unsigned int idxNode
                                                 )
{
    assert(idxNode < pTable->pCircuitNetList->noNodes);
    const unsigned int idxUnknown = pTable->nodeIdxToUnknownIdxAry[idxNode];
    if(idxUnknown != UINT_MAX)
        return &pTable->unknownLookUpAry[idxUnknown];
    else
        return NULL;

} /* End of tbv_getUnknownByNode */




/**
 * Look for an unknown variable related to a device current in the lookup table of unknowns.
 *   @return
 * The internal representation of the unknown is returned. If \a idxDevice is not the index
 * of a device, that defines an unknown current in the LES then an assertion fires.
 *   @param pTable
 * The reference to the filled table of symbolic objects is passed.
 *   @param idxDevice
 * The index of the device, the unknown is related to. The index refers to the array of devices
 * in the parse result. PCI_NULL_DEVICE must not be passed as it is not a unique device
 * identifier.
 */

const tbv_unknownVariable_t *tbv_getUnknownByDevice
                                    ( const tbv_tableOfVariables_t * const pTable
                                    , unsigned int idxDevice
                                    )
{
    assert(idxDevice < pTable->pCircuitNetList->noDevices);
    const unsigned int idxUnknown = pTable->devIdxToUnknownIdxAry[idxDevice];
    if(idxUnknown != UINT_MAX)
        return &pTable->unknownLookUpAry[idxUnknown];
    else
    {
        /* This case indicates an error in the client code. */
        assert(false);
        return NULL;
    }

} /* End of tbv_getUnknownByDevice */




/**
 * Look for a constant in the lookup table of those. The constant is identified by the
 * index of the device the constant belongs to.
 *   @return
 * The internal representation of the constant is returned. It's a bit vector with a single
 * set bit; this bit represents the constant.
 *   @param pTable
 * The reference to the filled table of symbolic objects is passed.
 *   @param idxDevice
 * The reference to the device to look for. A constant needs to be defined for this device
 * (\a tbv_addConstant). The reference is implemented as the index of the device in the
 * array of devices inside the parse result. (A read-only copy of the parse result is found
 * inside the object * \a pTable.)
 */

coe_productOfConst_t tbv_getConstantByDevice( const tbv_tableOfVariables_t * const pTable
                                            , unsigned int idxDevice
                                            )
{
    assert(idxDevice < pTable->pCircuitNetList->noDevices
           &&  pTable->devIdxToConstantIdxAry[idxDevice] < pTable->noConstants
          );

    /* Dynamically shape the required bit pattern by repeated shift. */
    coe_productOfConst_t constant = 0x01;
    unsigned int idxBit;
    for(idxBit=pTable->devIdxToConstantIdxAry[idxDevice]; idxBit>0; --idxBit)
        constant <<= 1;

    assert(constant != 0);
    return constant;

} /* End of tbv_getConstantByDevice */




/**
 * Look for a device description by the constant representing it.
 *   @return
 * The pointer to the device descriptor is returned.
 *   @param pTable
 * The reference to the filled table of symbolic objects is passed.
 *   @param idxBit
 * A device is represented by a "productOfConst" where one and only one bit is set. The
 * index of the set bit is passed as key for the lookup operation.
 */

const pci_device_t *tbv_getDeviceByBitIndex( const tbv_tableOfVariables_t * const pTable
                                           , unsigned int idxBit
                                           )
{
    const unsigned int idxDev = pTable->constantIdxToDevIdxAry[idxBit];
    assert(idxDev < pTable->pCircuitNetList->noDevices);
    return pTable->pCircuitNetList->pDeviceAry[idxDev];

} /* End of tbv_getDeviceByBitIndex */




/**
 * Look for the description of the device indirectly referenced by the constant representing
 * a device. Any device's value may be expressed as a product of a constant and the value
 * of another device. This may happen recursively. If such a reference or chain of
 * references is present for the device represented by the passed constant, than the last
 * device in the chain of references is returned, together with the factor of the values.
 *   @return
 * The other return values are valid only if the function returns true. If an error occurs
 * a message is written to the global application and false is returned.
 *   @param pTable
 * The reference to the filled table of symbolic objects is passed.
 *   @param pRefFactor
 * The effective factor of the value of the device directly addressed by \a idxBit to the
 * value of the finally referenced device ** \a ppDevice.
 *   @param ppDevice
 * The pointer to the descriptor of the finally referenced device is placed in * \a
 * ppDevice. In general, this is not the device addressed by \a idxBit.
 *   @param pIdxBitRefDev
 * A device is represented by a "productOfConst" where one and only one bit is set. The
 * index of the set bit for the finally referenced device is placed in * \a pIdxBitRefDev.
 *   @param idxBit
 * A device is represented by a "productOfConst" where one and only one bit is set. The
 * index of the set bit is passed as key for the look up operation of the first device in
 * the chain of references.
 */

boolean tbv_getReferencedDeviceByBitIndex( const tbv_tableOfVariables_t * const pTable
                                         , rat_num_t *pRefFactor
                                         , const pci_device_t * * const ppDevice
                                         , unsigned int *pIdxBitRefDev
                                         , unsigned int idxBit
                                         )
{
    boolean success = true;
    rat_num_t refFactor = RAT_ONE;
    const pci_circuit_t * const pNetList = pTable->pCircuitNetList;
    assert(idxBit < pTable->noConstants);
    unsigned int idxDev = pTable->constantIdxToDevIdxAry[idxBit];
    assert(idxDev < pNetList->noDevices);
    const pci_device_t *pDev = pNetList->pDeviceAry[idxDev];
    unsigned int noVisitedDevs = 1;
#ifdef DEBUG
    const pci_deviceType_t type = pDev->type;
#endif

    /* The global numeric overflow flag needs to be false for safe error recognition. */
    assert(!rat_getError());

    /* Loop as long as the device has a reference to another device. */
    while(success &&  pDev->devRelation.idxDeviceRef != PCI_NULL_DEVICE)
    {
        ++ noVisitedDevs;

        /* There may be cyclic references which would lead to an endless loop. The simplest
           way to recognize this builds on the limited number of symbolic constants. (Only
           devices having such a constant as representation of their value can reference
           each other.) */
        if(noVisitedDevs > COE_MAX_NO_CONST)
        {
            success = false;
            LOG_ERROR( _log
                     , "Cyclic references between devices' values found. One of the affected"
                       " devices is %s (%s)"
                     , pDev->name
                     , pci_getNameOfDeviceType(pDev)
                     );
        }

        /* Accumulate the factor of the reference and double-check against overflow. */
        if(success)
        {
            refFactor = rat_mul(refFactor, pDev->devRelation.factorRef);
            if(rat_getError())
            {
                success = false;
                LOG_ERROR( _log
                         , "Numeric overflow in the value of device %s (%s)"
                         , pDev->name
                         , pci_getNameOfDeviceType(pDev)
                         );
            }
        }

        /* Follow the reference to the next device in the chain. */
        if(success)
        {
            idxDev = pDev->devRelation.idxDeviceRef;
            assert(idxDev < pNetList->noDevices);
            pDev = pNetList->pDeviceAry[idxDev];
            assert(pDev->type == type);
        }
    } /* End while(Found device's value is related to the value of another device) */

    if(success)
    {
        *pRefFactor = refFactor;
        *ppDevice = pDev;

        /* Find the constant/bit index addressing to the finally referenced device. */
        assert(idxDev < pNetList->noDevices);
        *pIdxBitRefDev = pTable->devIdxToConstantIdxAry[idxDev];
    }
    else
    {
        rat_clearError();
        *pRefFactor = RAT_ONE;
        *ppDevice = NULL;
        *pIdxBitRefDev = 0;
    }

    return success;

} /* End of tbv_getReferencedDeviceByBitIndex */




/**
 * Select a specific unknown for result computation.\n
 *   Currently, the solver is not capable to find a solution for all unknowns at once. It
 * returns a fully eliminated solution only for the very unknown, whose coefficients are
 * placed in the rightmost LHS column m of the LES. By simply exchanging two columns of the
 * (later) LES on user demand, we can achieve that any unknown is represented in column
 * m of the LES. Column swapping is what this method actually does.
 *   @return
 * True if the desired unknown is known; then the operation succeeds. Otherwise false.
 *   @param pTable
 * The assignment of unknowns to columns in the LES is specified in a table of variables.
 * The pointer to such a table is passed. The table is modified in place.
 *   @param nameOfUnknown
 * The unknown is selected by name.
 */

boolean tbv_setTargetUnknownForSolver( tbv_tableOfVariables_t * const pTable
                                     , const char * const nameOfUnknown
                                     )
{
    tbv_unknownVariable_t * const unknownLookUpAry = pTable->unknownLookUpAry;

    /* Look for the selected target unknown. */
    unsigned int idxSelected = 0;
    for(idxSelected=0; idxSelected<pTable->noUnknowns; ++idxSelected)
        if(strcmp(unknownLookUpAry[idxSelected].name, nameOfUnknown) == 0)
            break;

    /* User error if it doesn't exist. */
    if(idxSelected >= pTable->noUnknowns)
    {
        LOG_ERROR( _log
                 , "Unknown %s selected for solution doesn't exist in the LES. This"
                   " unknown can't be selected as result of the solver"
                 , nameOfUnknown
                 );
        return false;
    }

    /* Look for the very unknown, which currently has the rightmost position in the LES. We
       are going to exchange the selected with this one. */
    unsigned int idxCurrent = 0;
    for(idxCurrent=0; idxCurrent<pTable->noUnknowns; ++idxCurrent)
        if(unknownLookUpAry[idxCurrent].idxCol == pTable->noUnknowns - 1)
            break;
    assert(idxCurrent < pTable->noUnknowns);

    /* The selected unknown could already be at the desired rightmost position. */
    if(idxSelected != idxCurrent)
    {
        unknownLookUpAry[idxCurrent].idxCol = unknownLookUpAry[idxSelected].idxCol;
        unknownLookUpAry[idxSelected].idxCol = pTable->noUnknowns - 1;
    }

    return true;

} /* End of tbv_setTargetUnknownForSolver */





