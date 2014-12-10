#ifndef TBV_TABLEOFVARIABLES_INCLUDED
#define TBV_TABLEOFVARIABLES_INCLUDED
/**
 * @file tbv_tableOfVariables.h
 * Definition of global interface of module tbv_tableOfVariables.c
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
#include "rat_rationalNumber.h"
#include "pci_parserCircuit.h"
#include "coe_coefficient.h"
#include "msc_mScript.h"


/*
 * Defines
 */


/*
 * Global type definitions
 */


/** The information describing a known variable of the LES. */
typedef struct
{
    /** The name of the known variable. */
    const char *name;

    /** The index of the column in the LES, which belongs to this known. */
    unsigned int idxCol;

} tbv_knownVariable_t;


/** The information describing an unknown variable of the LES. */
typedef struct
{
    /** The name of the unknown variable. */
    const char *name;

    /** If an unknown is the voltage of a node: the node's index. Otherwise
        PCI_NULL_NODE. */
    unsigned int idxNode;

    /** If an unknown is the current through a device: the device's index. Otherwise
        PCI_NULL_DEVICE. */
    unsigned int idxDevice;

    /** The index of the row in the LES, which belongs to this unknown. */
    unsigned int idxRow;

    /** The index of the column in the LES, which belongs to this unknown. */
    unsigned int idxCol;

    /** If idxNode is not PCI_NULL_NODE, i.e. if the unknown is related to a node's voltage
        potential, then this field holds the identification of the sub-net the node belongs
        to. */
    unsigned int idSubNet;

} tbv_unknownVariable_t;


/** A data structure, that holds lists of known and unknown variables of the LES and the
    constants, that are part of its coefficients. The data structure is used for various
    lookup operations. */
typedef struct tbv_tableOfVariables_t
{
    /** A counter of references to this object. Used to control deletion of object. */
    unsigned int noReferencesToThis;


    /** The size of the table of known variables. */
    unsigned int maxNoKnowns;

    /** The number of known variables. */
    unsigned int noKnowns;

    /** The lookup table for knowns. */
    tbv_knownVariable_t *knownLookUpAry;


    /** The size of the table of unknown variables. */
    unsigned int maxNoUnknowns;

    /** The number of unknown variables. */
    unsigned int noUnknowns;

    /** The lookup table for unknowns. */
    tbv_unknownVariable_t *unknownLookUpAry;


    /** The agreed number of constants in the system. */
    unsigned int maxNoConstants;

    /** The number of constants. */
    unsigned int noConstants;

    /** Some node properties are related to knowns and unknowns and are held in this
        object. A map permits to directly access these properties via the node's linear
        index in the circuit net list. */
    unsigned int *nodeIdxToUnknownIdxAry;
    
    /** Some device properties are related to knowns and are held in this object. A map
        permits to directly access these properties via the devices's linear index in the
        circuit net list. */
    unsigned int *devIdxToKnownIdxAry;
    
    /** Some device properties are related to unknowns and are held in this object. A map
        permits to directly access these properties via the devices's linear index in the
        circuit net list. */
    unsigned int *devIdxToUnknownIdxAry;
    
    /** The (physical) constants are related to (some of) the interconnected devices. These
        devices are referenced by index into the table of device in the nested net list *
        \a pCircuitNetList.\n
          The main property of a constant, the bit vector it is represented by in a product
        of constants, is stored implicitly only: index 0 means bit vector 0x1, index 1
        means 0x2, index 2 means 0x4, and so forth. */
    unsigned int *constantIdxToDevIdxAry;

    /** Some operations need to follow the link from a device in the net list * \a
        pCircuitNetList to a constant. Here we have the inverse lookup table, inverse to \a
        constantDevIdxAry. */
    unsigned int *devIdxToConstantIdxAry;


    /** The entries of the different tables refer to the net list representing the circuit,
        which holds details about nodes and interconnected devices. Here is a copy (by
        reference) to the related net list. */
    const pci_circuit_t *pCircuitNetList;

} tbv_tableOfVariables_t;


/** Function argument of \a tbv_exportAsMCode: The variables can be exported in different
    contexts of M code. Choose, which context to support. */
typedef enum { tbv_assignDefaultValues
             , tbv_assignParameterStruct
             , tbv_copyToParameterStruct } tbv_contextOfMCode_t;


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */

/** Initialize the module prior to first use of any of its methods or global data objects. */
void tbv_initModule(log_hLogger_t hGlobalLogger);

/** Shutdown of module after use. Release of memory, closing files, etc. */
void tbv_shutdownModule(void);

/** Create a table of variables, which is still empty. */
tbv_tableOfVariables_t *tbv_createTableOfVariables( unsigned int noKnowns
                                                  , unsigned int noUnknowns
                                                  , unsigned int noConstants
                                                  , const pci_circuit_t * const pCircuitNetList
                                                  );

/** Create a new object, which is a shallow copied of the passed one. */
tbv_tableOfVariables_t *tbv_cloneByShallowCopy
                                        (const tbv_tableOfVariables_t * const pExistingObj);

/** Get another reference to the same object. */
tbv_tableOfVariables_t *tbv_cloneByReference(tbv_tableOfVariables_t * const pTabOfVars);

/** Get another read-only reference to the same object. */
const tbv_tableOfVariables_t *tbv_cloneByConstReference
                                            (const tbv_tableOfVariables_t * const pTabOfVars);

/** Delete a table of variables or a reference to it after use. */
void tbv_deleteTableOfVariables(const tbv_tableOfVariables_t * const pTabOfVars);

/** Write the contents of a table of variables object to the application log. */
void tbv_logTableOfVariables(const tbv_tableOfVariables_t * const pTable);

/** Add a known variable to the table. */
boolean tbv_addKnown( tbv_tableOfVariables_t * const pTable
                    , const char * const name
                    , unsigned int idxDevice
                    );

/** Add an unknown variable to the table. */
boolean tbv_addUnknown( tbv_tableOfVariables_t * const pTable
                      , const char * const name
                      , unsigned int idxNode
                      , unsigned int idxSubNet
                      , unsigned int idxDevice
                      );

/** Add a constant to the table. */
void tbv_addConstant( tbv_tableOfVariables_t * const pTable, unsigned int idxDevice);

/** Sort constants to get the common order R before L before C. */
void tbv_sortConstants(tbv_tableOfVariables_t * const pTable);

/** Write the elements that are relevant to the executable Octave code into an M script. */
void tbv_exportAsMCode( const tbv_tableOfVariables_t * const pTable
                      , msc_mScript_t * const pMScript
                      , tbv_contextOfMCode_t context
                      , const char * const indentStr
                      );
                      
/** Get the reference to the known, which represents a constant source. */
const tbv_knownVariable_t *tbv_getKnownByDevice
                                    ( const tbv_tableOfVariables_t * const pTable
                                    , unsigned int idxDevice
                                    );

/** Get the reference to the unknown, which represents a specific node's voltage. */
const tbv_unknownVariable_t *tbv_getUnknownByNode( const tbv_tableOfVariables_t * const pTable
                                                 , unsigned int idxNode
                                                 );

/** Get the reference to the unknown, which represents the  current through a specific
    devices. */ 
const tbv_unknownVariable_t *tbv_getUnknownByDevice
                                    ( const tbv_tableOfVariables_t * const pTable
                                    , unsigned int idxDevice
                                    );

/** Lookup of a device's constant by the index of the device. */
coe_productOfConst_t tbv_getConstantByDevice( const tbv_tableOfVariables_t * const pTable
                                            , unsigned int idxDevice
                                            );

/** Lookup a device description by the it representing constant. */
const pci_device_t *tbv_getDeviceByBitIndex( const tbv_tableOfVariables_t * const pTable
                                           , unsigned int idxBit
                                           );

/** Look for the decription of the device indirectly referenced by the constant
    representing a device. */
boolean tbv_getReferencedDeviceByBitIndex( const tbv_tableOfVariables_t * const pTable
                                         , rat_num_t *pRefFactor
                                         , const pci_device_t * * const ppDevice
                                         , unsigned int *pIdxBitRefDev
                                         , unsigned int idxBit
                                         );

/** Select a specific unknown for result computation. */
boolean tbv_setTargetUnknownForSolver( tbv_tableOfVariables_t * const pTable
                                     , const char * const nameOfUnknown
                                     );

#endif  /* TBV_TABLEOFVARIABLES_INCLUDED */
