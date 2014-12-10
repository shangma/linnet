#ifndef PCI_PARSERCIR_INCLUDED
#define PCI_PARSERCIR_INCLUDED
/**
 * @file pci_parserCircuit.h
 * Definition of global interface of module pci_parserCircuit.c
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

#include <limits.h>

#include "types.h"
#include "log_logger.h"
#include "rat_rationalNumber.h"
#include "coe_coefficient.h"
#include "msc_mScript.h"


/*
 * Defines
 */

/** The maximum number of nodes. */
#define PCI_MAX_NO_NODES    200

/** The maximum number of branches. */
#define PCI_MAX_NO_BRANCHES 200

/** The maximum number of devices. */
#define PCI_MAX_NO_DEVICES  200

/** The invalid node index. Indication, that this is not a node. */
#define PCI_NULL_NODE   UINT_MAX

/** The invalid device index. Indication, that this is not a device. */
#define PCI_NULL_DEVICE UINT_MAX

/** The maximum number of reported output voltages. */
#define PCI_MAX_NO_VOLTAGE_DEFINITIONS  100

/** The maximum number of LTI (transfer function) results. */
#define PCI_MAX_NO_RESULTS      100

/** The maximum number of dependent and independent quantities in any user defined result.
    The limit is defined relative to other limits, but this is not a real constraint, any
    positive number can be set. */
#define PCI_MAX_SIZE_OF_RESULT_SETS ((COE_MAX_NO_CONST)+(PCI_MAX_NO_VOLTAGE_DEFINITIONS))


/*
 * Global type definitions
 */

/** The supported kinds of electronic devices, that may be connected to analysable
    circuits.\n
      This is basically an enumeration but the order of elements matters with respect to
    the result representation. If a product of symbols (as used in the coefficients of the
    LES) is printed then the factors are sorted in the order of rising enumeration values
    prior to the output. As an example, the resistor should come before the capacitor in
    order to get the common output of R*C rather than C*R.\n
      We want to see R-L-C. The constant sources, the op-amp and the current probe don't
    appear as a symbol in the coeffcients of the LES, so they don't matter in this context.
    The controlled sources often lead to results, which are interpreted as virtual change
    of the value of one of the physical values R, L or C, e.g. beta*R might appear in a
    bipolar transistor circuit. To support this interpretation it seems to be best to have
    the controlled sources prior to the constant physical devices.
      @remark Caution, any change of the enumeration needs an according adaptation of the
    enum-to-string conversion, see pci_parserCircuit.c:deviceTypeToString. */
typedef enum 
{ 
    pci_devType_srcU = 0
    , pci_devType_srcUByU
    , pci_devType_srcUByI
    , pci_devType_srcI
    , pci_devType_srcIByU
    , pci_devType_srcIByI
    , pci_devType_resistor
    , pci_devType_conductance
    , pci_devType_inductivity
    , pci_devType_capacitor
    , pci_devType_opAmp
    , pci_devType_currentProbe

    , pci_devType_invalid
    , pci_devType_noDeviceTypes = pci_devType_invalid

} pci_deviceType_t;


/** The relation of a device to another one of same kind. */
typedef struct
{
    /** The reference to another device is implemented as index into the array of all known
        devices. \a PCI_NULL_DEVICE means: "No relation to another device is specified." */
    unsigned int idxDeviceRef;

    /** The factor in which the devices are related. Allowed are positive, non zero
        rational numbers. */
    rat_num_t factorRef;

} pci_deviceRelation_t;


/** The description of a single device. */
typedef struct
{
    /** Which kind of device? */
    pci_deviceType_t type;

    /** The name, which is used for reporting and logging only. */
    const char *name;

    /** The nodes the device is connected to are implemented as indexes into the array of
        all known nodes. */
    unsigned int idxNodeFrom, idxNodeTo;

    /** The node the output of an operational amplifier is connetced to. Or PCI_NULL_NODE
        for all other devices. The node is implemented as index into the array of all known
        nodes. */
    unsigned int idxNodeOpOut;

    /** The control inputs for voltage controlled sources or PCI_NULL_NODE otherwise. The
        nodes are implemented as indexes into the array of all known nodes. */
    unsigned int idxNodeCtrlPlus, idxNodeCtrlMinus;

    /** For the current controlled sources only: They refer to a current probe device;
        their output current is proportional to the sensed current. The refernce is the
        linear index into the array of devices inside the parse result object or
        PCI_NULL_DEVICE for all devices other than current controlled sources. */
    unsigned int idxCurrentProbe;

    /** A device can be related to another one in a simple way, e.g. a resistor can be
        defined to have double the resistance of another one. */
    pci_deviceRelation_t devRelation;

    /** The numerical default value for later evaluation of the network analysis results.
        -1.0 means: "No numeric value is specified." */
    double numValue;
    
} pci_device_t;


/** The definition of an output voltage as a difference between two voltage potentials. */
typedef struct pci_voltageDef_t
{
    /** The name of the defined voltage is used for result reporting. */
    const char *name;

    /** The nodes the voltage refers to are implemented as indexes into the array of
        all known nodes. */
    unsigned int idxNodePlus, idxNodeMinus;

} pci_voltageDef_t;



/** Some information supporting result plotting. This information is not directly used
    but forwarded to the plot backend. */
typedef struct
{
    /** Should the frequency axis be linear or logarithmic? */
    boolean isLogX;

    /** The number of points of the plotted curve. */
    unsigned int noPoints;

    /** The frequency range the plot should be made for. From and to value in Hz. */
    double freqLimitAry[2];

} pci_plotInfo_t;



/** A user demanded result, an LTI system. Either a simple transfer function, which can be
    plotted as Bode diagram or a complete solution. */
typedef struct
{
    /** The name of the result. The name is used e.g. as part of the title of frequency
        response plots. */
    const char *name;

    /** The number of dependent quantities in the result. */
    unsigned int noDependents;

    /** The name(s) of the dependent quantities. Normally this is a set of unknowns. In the
        particular case of one dependent and one independent, this may also be a known if
        an inverse transfer function is requested. */
    const char *dependentNameAry[PCI_MAX_SIZE_OF_RESULT_SETS];

    /** The name of the independent quantity. Normally this is a known. In the particular
        case of one dependent and one independent quantity, this may also be an unknown if
        an inverse transfer function is requested.\n
          The value is NULL if a full result is demanded, which depends on all knows of the
        system. */
    const char *independentName;

    /** Some information supporting result plotting. This information is not directly used
        but forwarded to the plot backend. The information is optional; if not givven the
        pointer is NULL. */
    pci_plotInfo_t *pPlotInfo;

} pci_resultDef_t;


/** The complete parsing result, the complete circuit. */
typedef struct
{
    /** A counter of references to this object. Used to control deletion of object. */
    unsigned int noReferencesToThis;

    /** The number of different nodes used to interconnect the devices. */
    unsigned int noNodes;

    /** An array of \a noNodes names of nodes. */
    const char * nodeNameAry[PCI_MAX_NO_NODES];

    /** The number of different devices interconnected in the circuit. */
    unsigned int noDevices;

    /** An array of references to objects describing a device. */
    const pci_device_t * pDeviceAry[PCI_MAX_NO_DEVICES];

    /** The number of output voltages the user is interested in. */
    unsigned int noVoltageDefs;

    /** An array of \a noVoltageDefs voltage definitions. */
    pci_voltageDef_t voltageDefAry[PCI_MAX_NO_VOLTAGE_DEFINITIONS];

    /** The number of user defined LTI (Transfer function) results. */
    unsigned int noResultDefs;

    /** An array of \a noResultDefs user demanded results. */
    pci_resultDef_t resultDefAry[PCI_MAX_SIZE_OF_RESULT_SETS];

} pci_circuit_t;



/*
 * Global data declarations
 */


/*
 * Global prototypes
 */

/** Initialize the module prior to first use of any of its methods or global data objects. */
void pci_initModule(void);

/** Shutdown of module after use. Release of memory, closing files, etc. */
void pci_shutdownModule(void);

/** Parse the input file. */
boolean pci_parseCircuitFile( log_hLogger_t logger
                            , const pci_circuit_t * * const ppParseResult
                            , const char * const inputFileName
                            );

/** Get another reference to the same object. */
const pci_circuit_t *pci_cloneByConstReference(const pci_circuit_t * const pParseResult);

/** Free the parse result structure after usage. */
void pci_deleteParseResult(const pci_circuit_t *pParseResult);

/** The names of the supported types of a device are exported by this module for logging
    and reporting purpose. */
const char *pci_getNameOfDeviceType(const pci_device_t * const pDevice);

/** Render a plot information object as Octave script code. */
void pci_exportPlotInfoAsMCode( msc_mScript_t * const pMScript
                              , const pci_plotInfo_t * const pPlotInfo
                              , const char *indentStr
                              );
                              
#endif  /* PCI_PARSERCIR_INCLUDED */
