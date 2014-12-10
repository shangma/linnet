/**
 * @file pci_parserCircuit.c
 *   Parser for the circuit definition, *.cnl, file. All device definitions are read and
 * the network graph is stored as a linear list of branches. In parallel a list of device
 * names and sources is build together with a set of relations between devices of same
 * type. Additionally, the devices get their default numerical values.\n
 *   Caution, the implementation make intensive use of global data; the parser is in no
 * way reentrant. Only one parse process can be run at a time.
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
 *   pci_initModule
 *   pci_shutdownModule
 *   pci_parseCircuitFile
 *   pci_cloneByConstReference
 *   pci_deleteParseResult
 *   pci_getNameOfDeviceType
 *   pci_exportPlotInfoAsMCode
 * Local functions
 *   openInput
 *   createParseResult
 *   getToken
 *   enterNode
 *   deviceTypeToString
 *   findDevice
 *   sync
 *   parseListOfNodes
 *   enterDeviceDef
 *   disambiguateDeviceName
 *   parseRatNum
 *   parseDeviceRelation
 *   parseDevValueAssignment
 *   parseDeviceDef
 *   parseOldStyleOutput
 *   parsePlotInfo
 *   parseIdentifier
 *   parseVoltageDefintion
 *   parseResultDefintion
 *   checkNodeReference
 *   checkNodeReferences
 */

/*
 * Include files
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <math.h>
#include <assert.h>

#include "types.h"
#include "smalloc.h"
#include "snprintf.h"
#include "stricmp.h"
#include "rat_rationalNumber.h"
#include "pci_parserCircuit.h"
#include "tok_tokenStream.h"


/*
 * Defines
 */


/*
 * Local type definitions
 */

/** The underlying tokenizer knowns some built-in lexical tokens like numeric literals but
    this parser needs to define some addition tokens in order to descibe the formal syntax of
    a circuit file. */
enum { tokenTypePlotInfo = tok_tokenType_firstCustomToken
     , tokenTypeAssignment
     , tokenTypePlotLinAxis
     , tokenTypePlotLogAxis
     , tokenTypePlotLogAxisOld
     , tokenTypeVoltageDef
     , tokenTypeResultDef
     , tokenTypeBodeResultDef
     };


/*
 * Local prototypes
 */

static boolean getToken();


/*
 * Data definitions
 */

/** A global logger object is referenced from anywhere for writing progress messages. */
static log_hLogger_t _log = LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT;

/** For simplicity all functions read the next input token from a global stream object. */
static tok_hTokenStream_t _hTokenStream = TOK_HANDLE_TO_INVALID_TOKEN_STREAM;

/** For simplicity all functions find the current input token in a global object. */
static tok_token_t _token = TOK_UNINITIALIZED_TOKEN;

/** The overall parse result can be accessed from any nested sub-routine. We use a global
    variable to implement this. */
static boolean _parseError = false;

/** The parser still supports an old-fashioned style of circuit input files, which is
    compatible with the net list of an early release of the simulation tool SPICE.\n
      At run-time, this flag indicates to the parser's sub-routines, which input format to
    support. true means the new, standard format. */
static boolean _isStdFormat = false;

/* The number of voltage sources needs to be one and only one in the elder format; there it
   is the system input definition (i.e. the only independent of the only result) at the
   same time. This global variable is used to count the parsed definitions. */
static unsigned int _noOldStyleInputDefs = 0;

/** The parser is case insenitive for the elder format and case sensitive for the standard
    format. This is handled by using this global function pointer for all string tests. */
static signed int (*_strcmp)(const char *str1, const char *str2) = NULL;

#ifdef DEBUG
/** A global counter of all references to any created objects. Used to detect memory leaks. */
static unsigned int _noRefsToObjects = 0;
#endif

/*
 * Function implementation
 */


/**
 * Open (or close) the input file.
 *   @return
 * True if streams could be initialized as wanted, else false.
 *   @param fileName
 * Name of circuit file to be opened and parsed.
 *   @param open
 * Boolean flag. Pass true to open the streams and - after parsing - call again and pass
 * false to close the input file.
 */

static boolean openInput(const char * const fileName, boolean open)
{
    if(open)
    {
        assert(_hTokenStream == TOK_HANDLE_TO_INVALID_TOKEN_STREAM);

        /* hFile = NULL: Let class tok open and close the input file. */
        tok_hStream_t hStream = {.hFile = NULL};

        /* The tokenizer knows a number of built-in lexical tokens like number literals.
           Define the additional tokens a circuit file may consist of. */
        const tok_tokenDescriptor_t tokenDescriptorAry[] =
        {
            {"=",    tokenTypeAssignment},
            {"DEF",  tokenTypeVoltageDef},
            {"RES",  tokenTypeResultDef},
            {"PLOT", tokenTypeBodeResultDef},
            {".AC",  tokenTypePlotInfo},
            {"LOG",  tokenTypePlotLogAxis},
            {"DEC",  tokenTypePlotLogAxisOld},
            {"LIN",  tokenTypePlotLinAxis},
            {";",    tok_tokenTypeEndOfLine},
        };

        tok_tokenDescriptorTable_t tokenDescriptorTable =
                                   { .noTokenDescriptions = sizeof(tokenDescriptorAry)
                                                            / sizeof(tokenDescriptorAry[0])
                                   , .tokenDescriptorAry = tokenDescriptorAry
                                   , .startComment = "/*"
                                   , .endComment = "*/"
                                   , .startCommentTillEndOfLine = "//"
                                   };

        /* Open the token stream. No object is returned if this fails. */
        char *errMsg;
        boolean success = tok_createTokenStream( &_hTokenStream
                                               , &errMsg
                                               , fileName
                                               , hStream
                                               , /* customGetChar */ NULL
                                               , &tokenDescriptorTable
                                               );
        if(success)
        {
            assert(errMsg == NULL);
            tok_setBoolOption( _hTokenStream
                             , /* option */ tok_optionSuffixMultipliers
                             , /* value */  true
                             );
        }
        else
        {
            assert(_hTokenStream == TOK_HANDLE_TO_INVALID_TOKEN_STREAM);
            LOG_ERROR(_log, "Error opening circuit file. %s", errMsg)
            free(errMsg);
        }

        return success;
    }
    else
    {
        tok_deleteTokenStream(_hTokenStream);
        _hTokenStream = TOK_HANDLE_TO_INVALID_TOKEN_STREAM;

        const tok_token_t tmpToken = TOK_UNINITIALIZED_TOKEN;
        _token = tmpToken;

        return true;
    }
} /* End of openInput */




/**
 * Create an empty parse result.
 *   @return
 * A pointer to malloc allocated object is returned.
 */

static pci_circuit_t *createParseResult()
{
    pci_circuit_t *pParseResult = smalloc(sizeof(pci_circuit_t), __FILE__, __LINE__);

    /* The caller of the function gets the first reference to the new object. */
    pParseResult->noReferencesToThis = 1;
#ifdef DEBUG
    ++ _noRefsToObjects;
#endif

    pParseResult->noNodes = 0;
    pParseResult->noDevices = 0;
    pParseResult->noVoltageDefs = 0;
    pParseResult->noResultDefs = 0;

    return pParseResult;

} /* End of createParseResult */




/**
 * Read the next token from the input stream into the global variable. Emit an error
 * message if this fails.
 *   @return
 * True if no syntax or stream error appeared.
 *   @remark
 * The global error flag \a _parseError is set and a message is written to the log in case
 * of an error.
 */

static boolean getToken()
{
    /* Read next lexical atom from the input stream. */
    if(!tok_getNextToken(_hTokenStream, &_token))
    {
        _parseError = true;
        LOG_ERROR( _log
                 , "Line %u: Syntax error in lexical analysis. %s"
                 , tok_getLine(_hTokenStream)
                 , tok_getErrorMsg(_hTokenStream)
                 );

        /* Acknowledge the error after reporting. We continue parsing. */
        tok_resetError(_hTokenStream, /* pErrMsg */ NULL);

        return false;
    }
    else
        return true;

} /* End of getToken */




/**
 * Synchronize parser with input stream after an error. We read without understanding until
 * the next line or sentence. From here, we have a chance to see meaningful input again.
 *   @remark
 * The global error flag \a _parseError is set and a message is written to the log in case
 * of an error.
 */

static void sync()
{
    while(_token.type != EOF  &&  _token.type != tok_tokenTypeEndOfLine && getToken())
        ;

} /* End of sync */



/**
 * Find a node in the parse result - or enter it into the parse result if not found.
 *   @return
 * True if returned index is valid, otherwise false.
 *   @param pIdxNode
 * The index of the node in the array of all nodes is written into \a * pIdxNode. Or
 * PCI_NULL_NODE if the maximum number of supported nodes has been exceeded.
 *   @param pParseResult
 * The parse result as known so far.
 *   @param nodeName
 * The name of the node to look up. A malloc allocated string is expected. If the function
 * succeeds then * \a pParseResult takes the ownership. Otherwise the caller remains
 * responsible for freeing the string.
 *   @remark
 * The global error flag \a _parseError is set and a message is written to the log in case
 * of an error.
 */

static boolean enterNode( unsigned int *pIdxNode
                        , pci_circuit_t * const pParseResult
                        , const char * const nodeName
                        )
{
    assert(_parseError == false);

    /* No need to ever have something different as linear search: The number of actually
       possible nodes is very low. */
    unsigned int idxNode;
    for(idxNode=0; idxNode<pParseResult->noNodes; ++idxNode)
    {
        if(_strcmp(pParseResult->nodeNameAry[idxNode], nodeName) == 0)
        {
            *pIdxNode = idxNode;
            return true;
        }
    }

    /* Not found; add this node at the end if possible. */
    if(pParseResult->noNodes < PCI_MAX_NO_NODES)
    {
        pParseResult->nodeNameAry[pParseResult->noNodes] = nodeName;
        *pIdxNode = pParseResult->noNodes++;
        return true;
    }
    else
    {
        _parseError = true;
        LOG_FATAL( _log
                 , "Line %u: Maximum number %u of nodes exceeded"
                 , tok_getLine(_hTokenStream)
                 , PCI_MAX_NO_NODES
                 );
        *pIdxNode = PCI_NULL_NODE;
        return false;
    }
} /* End of enterNode */




/**
 * Query a readable name for a supported device type.
 *   @return
 * Get the name as a pointer to a constant character string.
 *   @param type
 * The device type.
 */

static const char *deviceTypeToString(pci_deviceType_t type)
{
    /* Hint: The global logger is not available for the implementation of this method.*/

    static const char *deviceTypeStrAry_[pci_devType_noDeviceTypes+1] =
         { [ 0] = "Voltage source, constant"
         , [ 1] = "Voltage source, controlled by voltage"
         , [ 2] = "Voltage source, controlled by current"
         , [ 3] = "Current source, constant"
         , [ 4] = "Current source, controlled by voltage"
         , [ 5] = "Current source, controlled by current"
         , [ 6] = "Resistor"
         , [ 7] = "Conductance"
         , [ 8] = "Inductivity"
         , [ 9] = "Capacitor"
         , [10] = "Op-Amp"
         , [11] = "Current probe"
         , [12] = "(invalid value)"
         };
    assert((unsigned int)type <= 12  &&  (int)pci_devType_invalid == 12);

    if((unsigned int)type <= (unsigned int)pci_devType_invalid)
        return deviceTypeStrAry_[(unsigned int)type];
    else
        return deviceTypeStrAry_[(unsigned int)pci_devType_invalid];

} /* End of deviceTypeToString */




/**
 * Serach for a device by type and name in the half-way completed parse result.
 *   @return
 * \a true if the device is found, \a false otherwise.
 *   @param pIdxDev
 * The index to the found device is placed in \a * pIdxDev. If the function fails then
 * PCI_NULL_DEVICE is returned.
 *   @param pParseResult
 * The searched parse result.
 *   @param devType
 * The demanded type of the device.
 *   @param devName
 * The demanded name of the device.
 */

static boolean findDevice( unsigned int * const pIdxDev
                         , const pci_circuit_t * const pParseResult
                         , pci_deviceType_t devType
                         , const char * const devName
                         )
{
    unsigned int u;
    const pci_device_t * const * ppDev = pParseResult->pDeviceAry;
    for(u=0; u<pParseResult->noDevices; ++u)
    {
        if((*ppDev)->type == devType  &&  strcmp((*ppDev)->name, devName) == 0)
        {
            *pIdxDev = u;
            return true;
        }

        ++ ppDev;
    }

    _parseError = true;
    *pIdxDev = PCI_NULL_DEVICE;
    LOG_ERROR( _log
             , "Line %u: The referenced device %s (%s) does not exist. Please note that"
               " forward references are not supported; the referenced device needs to"
               " be defined in a previous line"
             , tok_getLine(_hTokenStream)
             , devName
             , deviceTypeToString(devType)
             )
    return false;

} /* End of findDevice */




/**
 * Enter the parsed information concerning a device in the parse result structure. The next
 * element of the device array is filled.
 *   @return
 * \a true if operation succeeded, \a false in case of out of memory.
 *   @param pParseResult
 * The parse result is placed in \a * pParseResult.
 *   @param devType
 * The type of the device.
 *   @param devName
 * The name of the device. A malloc allocates string is expected. If the function succeeds
 * then * \a pParseResult takes the ownership. Otherwise the caller remains responsible for
 * freeing the string.
 *   @param idxNodeAry
 * The array of indexes of nodes the device is connected to. The device type determines how
 * many nodes have to be present in the array. The remaining array elements don't care.
 *   @param idxDevCurrentProbe
 * Only used for current controlled sources: The device index of the current probe, which
 * controles the source. PCI_NULL_DEVICE for all other devices.
 *   @param deviceValue
 * The device value to be used for simulation and plotting if specified or a negative value
 * otherwise.
 *   @param deviceRelation
 * The device value in relation to another device. An invalid reference is passed if no
 * such relation is specified in the input.
 *   @remark
 * The global error flag \a _parseError is set and a message is written to the log in case
 * of an error.
 */

static boolean enterDeviceDef( pci_circuit_t * const pParseResult
                             , pci_deviceType_t devType
                             , const char * const devName
                             , unsigned int idxNodeAry[4]
                             , unsigned int idxDevCurrentProbe
                             , double deviceValue
                             , const pci_deviceRelation_t deviceRelation
                             )
{
    assert(_parseError == false);

    pci_device_t dev;

    dev.idxNodeFrom = idxNodeAry[0];
    dev.idxNodeTo   = idxNodeAry[1];

    if(devType == pci_devType_opAmp)
        dev.idxNodeOpOut = idxNodeAry[2];
    else
        dev.idxNodeOpOut = PCI_NULL_NODE;

    if(devType == pci_devType_srcUByU  || devType == pci_devType_srcIByU)
    {
        dev.idxNodeCtrlPlus  = idxNodeAry[2];
        dev.idxNodeCtrlMinus = idxNodeAry[3];
    }
    else
    {
        dev.idxNodeCtrlPlus = PCI_NULL_NODE;
        dev.idxNodeCtrlMinus = PCI_NULL_NODE;
    }

    dev.type = devType;
    dev.name = devName; /* This string is malloc allocated. */

    if(devType == pci_devType_srcUByI  || devType == pci_devType_srcIByI)
        dev.idxCurrentProbe = idxDevCurrentProbe;
    else
        dev.idxCurrentProbe = PCI_NULL_DEVICE;

    /* Optional information about the value of the device. */
    dev.numValue = deviceValue;
    dev.devRelation = deviceRelation;
    assert(dev.numValue == -1.0  ||  dev.devRelation.idxDeviceRef == PCI_NULL_DEVICE);

    /* Success: Allocate a new object and add it to the list. */
    if(pParseResult->noDevices < PCI_MAX_NO_DEVICES)
    {
        pci_device_t *pNew = smalloc(sizeof(pci_device_t), __FILE__, __LINE__);
        *pNew = dev;
        pParseResult->pDeviceAry[pParseResult->noDevices++] = pNew;
        return true;
    }
    else
    {
        _parseError = true;
        LOG_FATAL( _log
                 , "Line %u: Maximum number %u of devices exceeded"
                 , tok_getLine(_hTokenStream)
                 , PCI_MAX_NO_DEVICES
                 );
        return false;
    }
} /* End of enterDeviceDef */




/**
 * Read a given number of node names from the input stream. The node names are registered
 * in the parse result (if the nodes are not yet known) and the indexes of the nodes in the
 * parse result are returned.
 *   @return
 * \a true if all nodes could be read from the input, \a false otherwise.
 *   @param pParseResult
 * The parsed node information is placed in \a * pParseResult.
 *   @param idxNodeAry
 * The indexes of the nodes with the parsed names are placed into this array. It has room
 * for at least \a noNodes entries. The contents are undefined if the function returns \a
 * false.
 *   @param noNodes
 * The expected number of nodes.
 *   @remark
 * The global error flag \a _parseError is set and a message is written to the log in case
 * of an error.
 */

static boolean parseListOfNodes( pci_circuit_t * const pParseResult
                               , unsigned int idxNodeAry[]
                               , unsigned int noNodes
                               )
{
    assert(_parseError == false);

    unsigned int u;
    for(u=0; u<noNodes; ++u)
    {
        if(_token.type == tok_tokenTypeIdentifier)
        {
            if(enterNode(&idxNodeAry[u], pParseResult, _token.value.identifier))
            {
                /* Only if the name string could be entered into the parse result we take
                   ownership of the malloc allocated identifier. Otherwise the scanner
                   remains in charge for freeing (as usual). */
                _token.value.identifier = NULL;
            }
            else
                return false;
        }
        else if(!_isStdFormat && _token.type == tok_tokenTypeInteger)
        {
            /* The old format permitted using any combination of digits and characters as
               node name. This can no longer be supported with the underlying scanner.
               However, the most typical use case of this syntax definition can be
               supported: Just using numbers as node identifiers. Ugly, but useful. */
            char nodeName[sizeof("4294967295\000")];
            snprintf(nodeName, sizeof(nodeName), "%lu", _token.value.integer);
            char *nameAlloc = stralloccpy(nodeName);
            LOG_WARN( _log
                    , "Line %u: Node name %s: The use of numbers as node names is"
                      " deprecated. Node names which are no valid identifiers are no"
                      " longer supported in general"
                    , tok_getLine(_hTokenStream)
                    , nodeName
                    )
            if(!enterNode(&idxNodeAry[u], pParseResult, nameAlloc))
            {
                free(nameAlloc);
                return false;
            }
        }
        else
        {
            _parseError = true;
            LOG_ERROR( _log
                     , _isStdFormat
                       ? "Line %u: Expect %u node references, failed to read the %u."
                         " one. Valid node names are defined like C/C++ identifiers"
                       : "Line %u: Expect %u node references, failed to read the %u."
                         " one. Please note that node names which are no valid C/C++"
                         " identifiers are no longer supported"
                     , tok_getLine(_hTokenStream)
                     , noNodes, u+1
                     )
            return false;
        }

        if(!getToken())
            return false;

    } /* End for(All expected nodes) */

    return true;

} /* End of parseListOfNodes */




/**
 * It's structurally basically possible to have different devices of same name. If devices
 * of same name are properly related (i.e. it's explicitly stated that they have the same
 * physical value) then it will even lead to a useful result. In all other cases the
 * result's representation is ambiguous and misleading (but not wrong). The old input
 * syntax supported devices of same name; today's standard format doesn't. To stay
 * compatible with the elder input format, identical names are changed now, it's not
 * considered an error with termination.
 *   @return
 * True if parsing can continue. The standard format forbids to have ambiguous names, the
 * function returns false if a forbidden ambiguity is recognized.
 *   @param pNewName
 * A malloc allocated pointer with a unique device name is returned in * \a pNewName. The
 * function can operate in place; if you pass a variable \a n as \a name then you may pass
 * the address of \a n as \a pNewName.
 *   @param pParseResult
 * The current state of parse result is checked for an identical name.
 *   @param name
 * The checked device name. A malloc allocated string is expected. If it is found to be
 * ambiguous it is made unique by appending a suffix. name is freed and then a new call of
 * malloc is made to allocate memory for the new, returned name.
 *   @remark
 * The global error flag \a _parseError is set and a message is written to the log in case
 * of an error.
 */

static boolean disambiguateDeviceName( const char * * const pNewName
                                     , const pci_circuit_t * const pParseResult
                                     , const char * const name
                                     )
{
    assert(_parseError == false);

    const char *newName = name;

    /* First attempt: Use line number to disambiguate; later attempts: use global ID. */
    boolean isUnique;
    unsigned int try = 0;
    do
    {
        isUnique = true;
        unsigned int idxDev;
        for(idxDev=0; idxDev<pParseResult->noDevices; ++idxDev)
        {
            if(_strcmp(pParseResult->pDeviceAry[idxDev]->name, newName) == 0)
            {
                /* The current name candidate is ambiguous, create the next candidate. */
                isUnique = false;

                /* Normally it should be sufficent to use the line number to disambiguate a
                   name but for pathologic situations we will also need a globally unique
                   ID. */
                static unsigned int globalIdxDev = 1;
                char nameCandidate[strlen(name) + 2*sizeof("_L4294967295")+1];
                if(try == 0)
                {
                    snprintf( nameCandidate
                            , sizeof(nameCandidate)
                            , "%s_L%02u"
                            , name
                            , tok_getLine(_hTokenStream)
                            );

                    /* First attempt: Pointer newName is identical to pointer name and is
                       not freed as name may be used for reporting below. We will free it
                       after reporting. */
                }
                else
                {
                    snprintf( nameCandidate
                            , sizeof(nameCandidate)
                            , "%s_L%02u_%u"
                            , name
                            , tok_getLine(_hTokenStream)
                            , globalIdxDev++
                            );

                    /* Free name candidate of first attempt. */
                    free((void*)newName);
                }
                newName = stralloccpy(nameCandidate);

                /* Re-start the outer loop. Check in second pass if name candidate is okay. */
                break;
            }
        } /* End for(Compare name candidate with all names already in use) */

        /* Count number of attempts. */
        ++ try;
    }
    while(!isUnique);
    assert(try <= PCI_MAX_NO_DEVICES+1);

    if(newName != name)
    {
        /* Renaming devices is not possible in general: The references, that relate the
           value of a device to the value of another device are done by name and they are
           undetermined if the referenced names are ambiguous. This is why the standard
           format forbids to have ambiguous names; the elder format ignored this problem
           and that's why we issue a warning only. Ambiguous references will silently
           (without further warning) address to the first appearance of a device name. */
        if(_isStdFormat)
        {
            _parseError = true;
            LOG_ERROR( _log
                     , "Line %u: Device name %s had been used before. Device names need to"
                       " be unique"
                     , tok_getLine(_hTokenStream)
                     , name
                     );
        }
        else
        {
            LOG_WARN( _log
                    , "Line %u: Device name %s had been used before. Device names should"
                      " be unique. The device is renamed to %s. If this device should be"
                      " referenced by the value definition of another device then the"
                      " reference will be resolved in an unintended way without any"
                      " further error message or warning"
                    , tok_getLine(_hTokenStream)
                    , name
                    , newName
                    );
        }
        free((void*)name);
    }

    *pNewName = newName;

    return !_parseError;

} /* End of disambiguateDeviceName */




/**
 * Parse a rational number, which is represented by a quotient of two (positive) integers.
 *   @return
 * \a true if operation succeeded, \a false in case of a syntax error. If \a false is
 * returned then a message has been written to the log and all other results are undefined.
 *   @param pNumerator
 * The numerator of the rational number is placed in * \a pNumerator.
 *   @param pDenominator
 * The denominator of the rational number is placed in * \a pDenominator.
 *   @param pIsQuotient
 * The rational number can be explicitly expressed as a quotient of two integers but for
 * denominator=1 it can also be expressed as a single integer. In the former case this flag
 * is to \a true, in the latter case it is set to \a false.
 */

static boolean parseRatNum( unsigned long *pNumerator
                          , unsigned long *pDenominator
                          , boolean *pIsQuotient
                          )
{
    boolean success = true;
    *pNumerator = 0;
    *pDenominator = 1;
    *pIsQuotient = false;
    const char *note = "";
    if(_token.type == tok_tokenTypeInteger)
    {
        *pNumerator = _token.value.integer;
        if(!getToken())
            return false;

        if(_token.type == '/')
        {
            *pIsQuotient = true;

            if(!getToken())
                return false;

            if(_token.type == tok_tokenTypeInteger)
            {
                *pDenominator = _token.value.integer;
                if(!getToken())
                    return false;
            }
            else
            {
                success = false;
                note = ". Missing denominator";
            }
        }
    }
    else
        success = false;

    if(!success)
    {
        _parseError = true;
        LOG_ERROR( _log
                 , "Line %u: Syntax error in rational number. Expect a positive"
                   " numeric value or the quotient of two such values%s"
                 , tok_getLine(_hTokenStream)
                 , note
                 )
    }

    /* The elder format doesn't support non-integer numbers. */
    if(!_isStdFormat && success && *pIsQuotient)
    {
        success = false;
        _parseError = true;
        LOG_ERROR( _log
                 , "Line %u: Syntax error. Rational numbers are not supported"
                 , tok_getLine(_hTokenStream)
                 , note
                 )
    }

    return success;

} /* End of parseRatNum */




/**
 * Parse the value of a device constant assignment. Either a number or a multiple of
 * another, already known device.
 *   @return
 * True if parsing succeeded, else false.
 *   @param pDeviceValue
 * If the function succeeds then the physical value of the device is placed in * \a
 * pDeviceValue if it is specified absolute.
 *   @param pDeviceRelation
 * The parse result is placed in * \a pDeviceRelation if the device is related to another,
 * already known.
 *   @param pParseResult
 * The parse result so far. Used to identify a referenced, already defined other device.
 *   @param devType
 * The kind of device under progress. Needed to double-check whether a referenced device is
 * of same type.
 *   @remark
 * The global error flag \a _parseError is set and a message is written to the log in case
 * of an error.
 */

static boolean parseDeviceRelation( double * const pDeviceValue
                                  , pci_deviceRelation_t * const pDeviceRelation
                                  , const pci_circuit_t * const pParseResult
                                  , pci_deviceType_t devType
                                  )
{
/* To avoid too complex (and anyway useless) relations between devices the range of
   numerator and denominator is limited to a reasonable range. Permitting arbitrary ranges
   would significantly increase the danger of a computational overflow. */
#define MAX_NUMBER  999

    boolean success = true;

    /* An integer number can either be the physical value of the device or the factor in a
       relation to another device. This can only be decided with the next token. */
    boolean isPhysicalValue = false;
    double deviceValue = -1.0;
    unsigned long numerator = 1
                , denominator = 1;
    const char *note = "";

    if(_isStdFormat &&  _token.type == '(')
    {
        /* A rational number that expresses the relation to another device can be enclosed
           in a pair of parenthesis. If we find this pattern then it can't be a physical
           device value. */
        isPhysicalValue = false;
        if(!getToken())
            return false;

        boolean isQuotient;
        if(!parseRatNum(&numerator, &denominator, &isQuotient))
            success = false;

        if(_token.type == ')')
        {
            if(!getToken())
                return false;
        }
        else
        {
            success = false;
            _parseError = true;
            note = ". Missing closing parenthesis";
        }
    }
    else if(_token.type == tok_tokenTypeFpn)
    {
        /* A floating point number must not be used to express the ration of device
           constants. If we find one we know it is a physical device value. */
        isPhysicalValue = true;
        deviceValue = _token.value.fpn;

        if(!getToken())
            return false;
    }
    else if(_token.type == tok_tokenTypeInteger)
    {
        /* The currently seen integer can still be both. Let's read it as a rational number
           and see. */
        boolean isQuotient;
        if(!parseRatNum(&numerator, &denominator, &isQuotient))
            success = false;

        if(isQuotient)
            isPhysicalValue = false;
        else if(success)
        {
            assert(denominator == 1);

            /* The rational number was expressed as a single integer and only the next
               token can decide whether it is a relation to another device or a physical
               value. */
            if(_token.type == '*')
                isPhysicalValue = false;
            else
            {
                isPhysicalValue = true;
                deviceValue = (double)numerator;
            }
        }
    }
    else if(_token.type == tok_tokenTypeIdentifier)
    {
        /* We have the most basic situation of a one-by-one device relation with an
           implicit factor of one. */
        isPhysicalValue = false;
        numerator = 1;
        denominator = 1;
    }
    else
    {
        success = false;
        _parseError = true;
        note = ". Positive number or opening parenthesis expected";
    }

    rat_num_t factor = RAT_ONE;
    if(success)
    {
        if(isPhysicalValue)
        {
            if(deviceValue == 0.0) 
            {
                LOG_WARN( _log
                        , "Line %u: Bad value 0 used in assignment"
                        , tok_getLine(_hTokenStream)
                        )
            }
        }
        else
        {
            if(numerator > 0  && numerator <= MAX_NUMBER
               &&  denominator > 0  &&  denominator <= MAX_NUMBER
              )
            {
                factor.n = (rat_signed_int)numerator;
                factor.d = (rat_signed_int)denominator;
            }
            else
            {
                success = false;
                _parseError = true;
                LOG_ERROR( _log
                         , "Line %u: Error in device relation. The relation of two devices"
                           " can be expressed by a single integer or by a quotient of"
                           " two integers. Both integers are in the range 1..%u"
                         , tok_getLine(_hTokenStream)
                         , MAX_NUMBER
                         )
                note = ". Range exceeded, see before";
            }
        } /* End if(Is assignment of physical device value?) */
    } /* End if(Still no error?) */

    /* Here, if we found a physical device value, we are through and only have to check if
       we reached the end of the line, otherwise the (later) error feedback will be
       misleading.
         If we've found the factor of a device relation then the next tokens needs to be
       the multiplication operator and the name of the related device. */
    unsigned int idxDeviceRef = PCI_NULL_DEVICE;
    if(!isPhysicalValue)
    {
        if(success)
        {
            if(_token.type != tok_tokenTypeIdentifier)
            {
                if(_token.type == '*')
                {
                    if(!getToken())
                        return false;
                }
                else
                {
                    success = false;
                    _parseError = true;
                    note = ". Missing multiplication operator between factor and device name";
                }
            }
        } /* End if(Still no error?) */

        if(success)
        {
            if(_token.type == tok_tokenTypeIdentifier)
            {
                const char * const nameRefDev = _token.value.identifier;

                /* Resolve the reference: The name of the referenced device is replaced by the
                   index into the table of already known devices. */
                unsigned int idxDev;
                for(idxDev=0; idxDev<pParseResult->noDevices; ++idxDev)
                {
                    if(_strcmp(pParseResult->pDeviceAry[idxDev]->name, nameRefDev) == 0)
                    {
                        idxDeviceRef = idxDev;
                        break;
                    }
                }
                if(idxDeviceRef == PCI_NULL_DEVICE)
                {
                    success = false;
                    _parseError = true;
                    LOG_ERROR( _log
                             , _isStdFormat
                               ? "Line %u: Unknown device %s referenced. Forward references"
                                 " are not supported; a referenced device needs to be defined"
                                 " before"
                               : "Line %u: Unknown device %s referenced. Forward references"
                                 " are not supported; a referenced device needs to be defined"
                                 " before. The bad reference may also be caused by the"
                                 " disambiguation of doubly defined names. Please refer to"
                                 " previous messages"
                             , tok_getLine(_hTokenStream)
                             , nameRefDev
                             )
                    note =". Bad device reference, see before";
                }
                else if(pParseResult->pDeviceAry[idxDeviceRef]->type != devType)
                {
                    success = false;
                    _parseError = true;
                    idxDeviceRef = PCI_NULL_DEVICE;
                    LOG_ERROR( _log
                             , "Line %u: The referenced device %s is of other kind than the"
                               " it referencing device"
                             , tok_getLine(_hTokenStream)
                             , nameRefDev
                             )
                    note =". Bad device reference, see before";
                }

                if(!getToken())
                    return false;
            }
            else
            {
                success = false;
                _parseError = true;
                note = ". Name of referenced, related device expected";
            }
        } /* End if(Still no error?) */
    } /* End if(Device relation found?) */

    if(!success)
    {
        assert(_parseError == true);
        LOG_ERROR( _log
                 , "Line %u: Syntax error in value assignment%s. The value assignment is"
                   " either a positive numeric (physical) value or the product of a positive"
                   " %s and the name of a referenced, already defined device"
                 , tok_getLine(_hTokenStream)
                 , note
                 , _isStdFormat? "rational number (like 1/2)": "integer number"
                 )
        return false;
    }

    if(success &&  _token.type != tok_tokenTypeEndOfLine
       &&  _token.type != tok_tokenTypeEndOfFile
      )
    {
        success = false;
        _parseError = true;

        if(isPhysicalValue)
        {
            note = ". Please note that floating point numbers must not be used to express"
                   " a device relation. Use a ratio of integers instead";
        }
        LOG_ERROR( _log
                 , "Line %u: Syntax error. Unexpected characters found behind a value"
                   " assignment%s"
                 , tok_getLine(_hTokenStream)
                 , note
                 )
        return false;
    }

    /* Return the found information. */
    assert(deviceValue == -1.0  ||  idxDeviceRef == PCI_NULL_DEVICE);
    assert(deviceValue >= 0.0  ||  idxDeviceRef != PCI_NULL_DEVICE);
    assert(idxDeviceRef == PCI_NULL_DEVICE
           ||  (factor.n != 0  &&  factor.d != 0  &&  rat_sign(factor) > 0)
          );
    *pDeviceValue = deviceValue;
    pDeviceRelation->idxDeviceRef = idxDeviceRef;
    pDeviceRelation->factorRef = factor;

    return true;

#undef MAX_NUMBER
} /* End of parseDeviceRelation */




/**
 * Parse the (optional) appendix to a device definition that specifies the value of the
 * physical constant of that device. Used for result simplification and plotting.
 *   @return
 * True if parsing succeeded, else false.
 *   @param pAssignmentFound
 * The value definition is optional. If one is found then true is placed in * \a
 * pAssignmentFound, otherwise false. The other output variables have not been written if *
 * \a pAssignmentFound is false after return.
 *   @param pDeviceValue
 * The physical value of the device is placed in * \a pDeviceValue if it is specified
 * absolute.
 *   @param pDeviceRelation
 * The parse result is placed in * \a pDeviceRelation if the device is related to another,
 * already known.
 *   @param pParseResult
 * The parse result so far. Used to identify a referenced, already defined other device.
 *   @param devType
 * The syntax of the term depends on the type of the device.
 *   @param devName
 * The term may refer to the device by name. Pass the name to enable a cross check.
 *   @remark
 * The global error flag \a _parseError is set and a message is written to the log in case
 * of an error.
 */

static boolean parseDevValueAssignment( boolean *pAssignmentFound
                                      , double * const pDeviceValue
                                      , pci_deviceRelation_t * const pDeviceRelation
                                      , const pci_circuit_t * const pParseResult
                                      , pci_deviceType_t devType
                                      , const char * const devName
                                      )
{
    assert(_parseError == false);

    /* Op-amps do not have a physical value. They are considered ideal.
         Constant sources don't allow to define a physical value. They only define
       independent system inputs (and the dependent system output in the elder format). */
    *pAssignmentFound = !(devType == pci_devType_opAmp  ||  devType == pci_devType_currentProbe
                          ||  devType == pci_devType_srcI  ||  devType == pci_devType_srcU
                         );

    if(*pAssignmentFound)
    {
        /* The complete term is optional in both syntax format. The standard format starts
           with the name of the device (an identifier), the elder format requires a
           more complex consideration as the controlled current source uses a deviating
           syntax (for historic, unexplained reasons). */
        if(_isStdFormat)
            *pAssignmentFound = _token.type == tok_tokenTypeIdentifier;
        else
        {
            *pAssignmentFound =
              (devType != pci_devType_srcIByU  &&  _token.type == tok_tokenTypeIdentifier)
              ||  (devType == pci_devType_srcIByU  &&  _token.type != tok_tokenTypeEndOfLine
                   &&  _token.type != tok_tokenTypeEndOfFile
                  );
        }
    }

    if(*pAssignmentFound)
    {
        /* The standard format. */
        if(_isStdFormat)
        {
            assert(_token.type == tok_tokenTypeIdentifier);
            if(_strcmp(_token.value.identifier, devName) != 0)
            {
                _parseError = true;
                LOG_ERROR( _log
                         , "Line %u: Syntax error in value assignment. A value"
                           " assignment starts with the name of the device it belongs to"
                         , tok_getLine(_hTokenStream)
                         )
                return false;
            }

            if(!getToken())
                return false;

            if(_token.type != tokenTypeAssignment)
            {
                _parseError = true;
                LOG_ERROR( _log
                         , "Line %u: Syntax error in value assignment. Expect a '='"
                         , tok_getLine(_hTokenStream)
                         )
                return false;
            }

            if(!getToken())
                return false;
        }
        else
        {
            assert((devType != pci_devType_srcIByU  &&  _token.type == tok_tokenTypeIdentifier)
                   ||  (devType == pci_devType_srcIByU
                        &&  _token.type != tok_tokenTypeEndOfLine
                        &&  _token.type != tok_tokenTypeEndOfFile
                       )
                  );

            /* For obscure reasons did the elder format require a repetition of the
               device type character for all quantifiable devices but the voltage
               controlled current source. */
            if(devType != pci_devType_srcIByU)
            {
                /* Parse the LHS, e.g. R= */
                assert(_token.type == tok_tokenTypeIdentifier);
                boolean okay = strlen(_token.value.identifier) == 1;
                if(okay)
                {
                    const char devTypeChar = toupper(_token.value.identifier[0]);
                    okay = (devTypeChar == 'R'  &&  devType == pci_devType_resistor)
                           || (devTypeChar == 'Y'  &&  devType == pci_devType_conductance)
                           || (devTypeChar == 'C'  &&  devType == pci_devType_capacitor)
                           || (devTypeChar == 'L'  &&  devType == pci_devType_inductivity);
                }
                if(!okay)
                {
                    _parseError = true;
                    LOG_ERROR( _log
                             , "Line %u: Syntax error in value assignment. A value"
                               " assignment starts with the single character that"
                               " specifies the device type (one out of RYCL). The"
                               " character needs to match the type of the device the"
                               " assignment belongs to"
                             , tok_getLine(_hTokenStream)
                             )
                    return false;
                }

                if(!getToken())
                    return false;

                if(_token.type != tokenTypeAssignment)
                {
                    _parseError = true;
                    LOG_ERROR( _log
                             , "Line %u: Syntax error in value assignment. Expect a '='"
                             , tok_getLine(_hTokenStream)
                             )
                    return false;
                }

                if(!getToken())
                    return false;

            } /* End if(Passive device requiring an introductory term like R= ?) */

        } /* End if(Which syntax format?) */

        if(!parseDeviceRelation(pDeviceValue, pDeviceRelation, pParseResult, devType))
            return false;

    } /* End if(Optional value assignment is present in input stream?) */


    /* The feedback is better for the user if we now double check that we really reached
       the end of a line. */
    if(_token.type != tok_tokenTypeEndOfLine  &&  _token.type != tok_tokenTypeEndOfFile)
    {
        _parseError = true;

        const char *note;
        if(!_isStdFormat &&  devType == pci_devType_srcU)
        {
            note = ". Please note that the system input definition can't have a value"
                   " assignment";
        }
        else if(devType == pci_devType_opAmp  ||  devType == pci_devType_srcU
                ||  devType == pci_devType_srcI  ||  devType == pci_devType_currentProbe
               )
        {
            note = ". Please note that constant sources, operational amplifiers and"
                   " current probes can't have a value assignment";
        }
        else
            note = "";

        LOG_ERROR( _log
                 , "Line %u: Syntax error. Unexpected characters found at the end of"
                   " a device definition%s"
                 , tok_getLine(_hTokenStream)
                 , note
                 )
    }

    return !_parseError;

} /* End of parseDevValueAssignment */




/**
 * Parse a line that contains the defintion of a single device.
 *   @return
 * True if parsing succeeded, else false.
 *   @param pParseResult
 * The parse result is placed in \a * pParseResult.
 *   @remark
 * The global error flag \a _parseError is set and a message is written to the log in case
 * of an error.
 */

static boolean parseDeviceDef(pci_circuit_t * const pParseResult)
{
    /* On entry, the token shows the kind of device. */
    assert(_token.type == tok_tokenTypeIdentifier);

    const char *devName = NULL;
    pci_deviceType_t devType = pci_devType_invalid;

    /* Figure out what kind of device and how many nodes are expected. */
    assert(strlen(_token.value.identifier) >= 1);
    unsigned int noNodes = 0;

    if(_isStdFormat)
    {
        /* There are constraints on the device qualifier, which should have been already
           double-checked by the caller. */
        assert(strlen(_token.value.identifier) == 1
               ||  _strcmp(_token.value.identifier, "OP") == 0
               ||  _strcmp(_token.value.identifier, "PI") == 0
              );

        switch(_token.value.identifier[0])
        {
            case 'R': devType = pci_devType_resistor; noNodes = 2; break;
            case 'Y': devType = pci_devType_conductance; noNodes = 2; break;
            case 'C': devType = pci_devType_capacitor; noNodes = 2; break;
            case 'L': devType = pci_devType_inductivity; noNodes = 2; break;
            case 'O': devType = pci_devType_opAmp; noNodes = 3; break;
            case 'P': devType = pci_devType_currentProbe; noNodes = 2; break;
            case 'U': devType = pci_devType_srcU; noNodes = 2; break;
            case 'I': devType = pci_devType_srcI; noNodes = 2; break;
            default: assert(false);

        } /* End switch(Which kind of passive element?) */

        if(!getToken())
            return false;

        /* Sources need special handling; they may be controlled by a second pair of nodes. */
        if(_token.type == '(')
        {
            if(devType != pci_devType_srcU  &&  devType != pci_devType_srcI)
            {
                _parseError = true;
                LOG_ERROR( _log
                         , "Line %u: Only devices of kind U or I can have a reference to"
                           " a voltage or current for control"
                         , tok_getLine(_hTokenStream)
                         )
                return false;
            }

            /* Advance token to the controlling quantity. */
            if(!getToken())
                return false;

            if(_token.type != tok_tokenTypeIdentifier
               || strlen(_token.value.identifier) != 1
               || strchr("UI", _token.value.identifier[0]) == NULL
              )
            {
                _parseError = true;
                LOG_ERROR( _log
                         , "Line %u: A controlled source X is characterized by either"
                           "X(U) or X(I)"
                         , tok_getLine(_hTokenStream)
                         )
                return false;
            }

            /* Modify the device type, now we know it better. */
            if(_token.value.identifier[0] == 'U')
            {
                if(devType == pci_devType_srcU)
                    devType = pci_devType_srcUByU;
                else
                    devType = pci_devType_srcIByU;
            }
            else
            {
                if(devType == pci_devType_srcU)
                    devType = pci_devType_srcUByI;
                else
                    devType = pci_devType_srcIByI;
            }

            /* Advance the token beyond the closing bracket. */
            if(!getToken() ||  _token.type != ')')
            {
                _parseError = true;
                LOG_ERROR( _log
                         , "Line %u: A controlled source X is characterized by either X(U) or"
                           " X(I), closing bracket missing"
                         , tok_getLine(_hTokenStream)
                         )
                return false;
            }
            if(!getToken())
                return false;

            /* A voltage controlled source has two additional inputs for the controlling
               voltage. */
            if(devType == pci_devType_srcUByU  ||  devType == pci_devType_srcIByU)
            {
                assert(noNodes == 2);
                noNodes = 4;
            }
        } /* End if(Do we have a controlled source?) */

        /* Read the name of the device. */
        if(_token.type == tok_tokenTypeIdentifier)
        {
            /* Take ownership of malloc allocated identifier. */
            devName = _token.value.identifier;
            _token.value.identifier = NULL;
        }
        else
        {
            _parseError = true;
            LOG_ERROR( _log
                     , "Line %u: Failed to read the name of the device"
                     , tok_getLine(_hTokenStream)
                     )
            return false;
        }
    }
    else
    {
         /* Old input format. Type and name are encoded in the same token. */
        switch(toupper(_token.value.identifier[0]))
        {
            case 'R': devType = pci_devType_resistor; noNodes = 2; break;
            case 'Y': devType = pci_devType_conductance; noNodes = 2; break;
            case 'C': devType = pci_devType_capacitor; noNodes = 2; break;
            case 'L': devType = pci_devType_inductivity; noNodes = 2; break;
            case 'O': devType = pci_devType_opAmp; noNodes = 3; break;
            case 'U': devType = pci_devType_srcU; noNodes = 2; break;
            case 'S':
            case 'G': devType = pci_devType_srcIByU; noNodes = 4; break;
            default: assert(false);

        } /* End switch(Which kind of passive element?) */

        /* There are constraints on the names, which should have been already
           double-checked by the caller. */
        assert(devType != pci_devType_opAmp  ||  _strcmp(_token.value.identifier, "OP") == 0);
        assert(devType != pci_devType_srcU   ||  _strcmp(_token.value.identifier, "U1") == 0);

        /* Take ownership of malloc allocated identifier.
             Remark: The name of an op-amp is always "OP"; the name disambiguation below
           will always have to modify the name if more than one op-amp is used in the
           circuit. */
        devName = _token.value.identifier;
        _token.value.identifier = NULL;

        if(devType == pci_devType_srcU)
        {
            /* The elder format used to be case insensitive. A Bode plot result is defined
               below that refers to the input voltage definition "U1". Unify the name here
               in order to avoid unresolved references. */
            assert(toupper(*devName) == 'U');
            *(char*)devName = 'U';

            /* The number of voltage sources needs to be one and only one in the elder format;
               there it is the system input definition (i.e. the only independent of the only
               result) at the same time. A global variable is used to double-check this, a
               possible error is emitted at the end. */
            ++ _noOldStyleInputDefs;
        }
    } /* End if(Which input format?) */

    boolean success = getToken();

    /* Read the number of nodes references. */
    assert(noNodes >= 2  &&  noNodes <= 4);
    unsigned int idxNodeAry[4];
    if(success)
        success = parseListOfNodes(pParseResult, idxNodeAry, noNodes);

    /* A current controlled source references the it controlling current probe by name. */
    unsigned int idxDevCurrentProbe = PCI_NULL_DEVICE;
    if(success && (devType == pci_devType_srcUByI  ||  devType == pci_devType_srcIByI))
    {
        const char *currentProbeName = NULL;
        if(_token.type == tok_tokenTypeIdentifier)
        {
            /* Take ownership of malloc allocated identifier. */
            currentProbeName = _token.value.identifier;
            _token.value.identifier = NULL;

            success = getToken();
        }
        else
        {
            success = false;
            _parseError = true;
            LOG_ERROR( _log
                     , "Line %u: Failed to read the name of the current probe that controls"
                       " the current source %s. Forward references are not supported."
                       " A current probe is a previously defined device of kind PI"
                     , tok_getLine(_hTokenStream)
                     , devName
                     )
        }

        /* Try to resolve the reference. */
        if(success)
        {
            success = findDevice( &idxDevCurrentProbe
                                , pParseResult
                                , pci_devType_currentProbe
                                , currentProbeName
                                );
        }

        /* The name of the probe is needed only temporarily. */
        if(currentProbeName != NULL)
            free((char*)currentProbeName);

    } /* End if(Need to read a referenced current probe's name?) */

    /* The value of a device can be specified. */
    double deviceValue = -1.0;
    pci_deviceRelation_t deviceRelation = { .idxDeviceRef = PCI_NULL_DEVICE
                                          , .factorRef = RAT_NULL
                                          };
    if(success)
    {
        boolean assignmentFound;
        success = parseDevValueAssignment( &assignmentFound
                                         , &deviceValue
                                         , &deviceRelation
                                         , pParseResult
                                         , devType
                                         , devName
                                         );
    }

    /* Check for doubly defined names. They are structurally not a problem, but the
       final result would not be readable and useful at all. Change (and reallocate) the
       name in case. */
    if(success)
        success = disambiguateDeviceName(&devName, pParseResult, devName);

    /* Parsing succeeded: We have the device type, its name and the 2..4 nodes it is
       connected to. Put the information into the parse result. */
    if(success)
    {
        success = enterDeviceDef( pParseResult
                                , devType
                                , devName
                                , idxNodeAry
                                , idxDevCurrentProbe
                                , deviceValue
                                , deviceRelation
                                );
    }

    /* Do some result reporting. */
    if(success && log_checkLogLevel(_log, log_debug))
    {
        const char *nodeNameAry[4];
        unsigned int u;
        for(u=0; u<noNodes; ++u)
            nodeNameAry[u] = pParseResult->nodeNameAry[idxNodeAry[u]];
        for(   ; u<4; ++u)
            nodeNameAry[u] = "(n.c.)"; /* Not connected */

        if(deviceRelation.idxDeviceRef != PCI_NULL_DEVICE)
        {
            LOG_DEBUG( _log
                     , "Line %u: Found device definition: %s, %s (%u),"
                       " connected to %s, %s, %s, %s, value is (%ld/%ld)*%s"
                     , tok_getLine(_hTokenStream)
                     , devName
                     , deviceTypeToString(devType)
                     , (unsigned)devType
                     , nodeNameAry[0], nodeNameAry[1], nodeNameAry[2], nodeNameAry[3]
                     , deviceRelation.factorRef.n
                     , deviceRelation.factorRef.d
                     , pParseResult->pDeviceAry[deviceRelation.idxDeviceRef]->name
                     )
        }
        else if(deviceValue != -1.0)
        {
            LOG_DEBUG( _log
                     , "Line %u: Found device definition: %s, %s (%u),"
                       " connected to %s, %s, %s, %s, value is %f"
                     , tok_getLine(_hTokenStream)
                     , devName
                     , deviceTypeToString(devType)
                     , (unsigned)devType
                     , nodeNameAry[0], nodeNameAry[1], nodeNameAry[2], nodeNameAry[3]
                     , deviceValue
                     )
        }
        else
        {
            LOG_DEBUG( _log
                     , "Line %u: Found device definition: %s, %s (%u),"
                       " connected to %s, %s, %s, %s, no value is specified"
                     , tok_getLine(_hTokenStream)
                     , devName
                     , deviceTypeToString(devType)
                     , (unsigned)devType
                     , nodeNameAry[0], nodeNameAry[1], nodeNameAry[2], nodeNameAry[3]
                     )
        }
    } /* End if(Result reporting demanded?) */

    /* In case of success the malloc allocated name is now owned by the updated parse
       result structure. Otherwise we need to free it. */
    if(!success)
    {
        assert(devName != NULL);
        free((void*)devName);
    }

    return success;

} /* End of parseDeviceDef */




/**
 * Parse the definition of the output voltage as defined for the old input syntax.
 *   @return
 * The function returns true if the definition could be parsed error free. If it returns
 * false than all according error messages and hints have been written to the log file.
 *   @param pParseResult
 * The parse result is placed in \a * pParseResult if the function succeeds.
 *   @remark
 * The global error flag \a _parseError is set and a message is written to the log in case
 * of an error.
 */

static boolean parseOldStyleOutput(pci_circuit_t * const pParseResult)
{
    unsigned int idxNodeAry[2];
    if(!parseListOfNodes(pParseResult, idxNodeAry, /* noNodes */ 2))
    {
        _parseError = true;
        LOG_ERROR( _log
                 , "Line %u: Definition of output voltage U2 requires the specification"
                   " of two nodes. Output voltage is undefined"
                 , tok_getLine(_hTokenStream)
                 )
        return false;
    }

    /* The feedback is better for the user if we now double check that we really reached
       the end of a line (the user could try to add a value assignment). */
    if(_token.type != tok_tokenTypeEndOfLine  &&  _token.type != tok_tokenTypeEndOfFile)
    {
        _parseError = true;
        LOG_ERROR( _log
                 , "Line %u: Syntax error. Unexpected characters found at the end of"
                   " a device definition. Please note that the system output definition"
                   " can't have a value assignment"
                 , tok_getLine(_hTokenStream)
                 )
    }

    /* The old style syntax only allows a single output voltage. */
    if(pParseResult->noVoltageDefs == 0)
    {
        pParseResult->voltageDefAry[0].name = stralloccpy("U2");
        pParseResult->voltageDefAry[0].idxNodePlus = idxNodeAry[0];
        pParseResult->voltageDefAry[0].idxNodeMinus = idxNodeAry[1];
        pParseResult->noVoltageDefs = 1;
        return true;
    }
    else
    {
        _parseError = true;
        LOG_ERROR( _log
                 , "Line %u: Maximum number of outputs exceeded. The ckt input format"
                   " only permits a single output voltage definition"
                 , tok_getLine(_hTokenStream)
                 )
        return false;
    }
} /* End of parseOldStyleOutput */




/**
 * Parse the plot supporting information, which is a self-contained syntax element in the
 * old format and optional part of a result definition in the standard format.
 *   @return
 * Get either a malloc allocated plot info object or NULL in case of errors. Sufficient
 * error information has been written to the log if NULL is returned.
 *   @remark
 * Differently to many other parse function this function expects to still see the first
 * token of the plot info, which had been used to recognize that there is such a plot info.
 * Do not advance the token stream beyond the initial .AC, DEC, or LOG prior to the call of
 * this function.
 *   @remark
 * The global error flag \a _parseError is set and a message is written to the log in case
 * of an error.
 */

static pci_plotInfo_t *parsePlotInfo()
{
    assert(!_parseError);

    if(!_isStdFormat)
    {
        assert(_token.type == tokenTypePlotInfo);
        getToken();
    }

    /* Next token: Indicates whether to use linear or logarithmic frequency scale for plots. */
    const tok_tokenType_t tokenTypeLin = tokenTypePlotLinAxis
                        , tokenTypeLog = _isStdFormat
                                         ? tokenTypePlotLogAxis
                                         : tokenTypePlotLogAxisOld;
    const boolean isLogX = _token.type == tokenTypeLog;
    if(!isLogX &&  _token.type != tokenTypeLin)
    {
        _parseError = true;

        const char * const keywordLin = "LIN"
                 , * const keywordLog = _isStdFormat? "LOG": "DEC";
        LOG_ERROR( _log
                 , "Line %u: Plot info: Expect format of frequency scale, state either"
                   " %s or %s"
                 , tok_getLine(_hTokenStream)
                 , keywordLin
                 , keywordLog
                 )
    }
    else
        getToken();

    unsigned int noPoints = 0;
    if(!_parseError)
    {
        if(_token.type == tok_tokenTypeInteger)
        {
            noPoints = _token.value.integer;
            getToken();
        }
        else
        {
            _parseError = true;
            LOG_ERROR( _log
                     , "Line %u: Plot info: Expect integer number of frequency points"
                       " to plot as first one out of three positive numerics"
                     , tok_getLine(_hTokenStream)
                     )
        }
    }

    double fMin = 0.0;
    if(!_parseError)
    {
        if(_token.type == tok_tokenTypeFpn)
        {
            fMin = _token.value.fpn;
            getToken();
        }
        else if(_token.type == tok_tokenTypeInteger)
        {
            fMin = (double)_token.value.integer;
            getToken();
        }
        else
        {
            _parseError = true;
            LOG_ERROR( _log
                     , "Line %u: Plot info: Expect lower frequency boundary"
                       " as second one out of three positive numerics"
                     , tok_getLine(_hTokenStream)
                     )
        }
    }

    double fMax = 0.0;
    if(!_parseError)
    {
        if(_token.type == tok_tokenTypeFpn)
        {
            fMax = _token.value.fpn;
            getToken();
        }
        else if(_token.type == tok_tokenTypeInteger)
        {
            fMax = (double)_token.value.integer;
            getToken();
        }
        else
        {
            _parseError = true;
            LOG_ERROR( _log
                     , "Line %u: Plot info: Expect upper frequency boundary"
                       " as third one out of three positive numerics"
                     , tok_getLine(_hTokenStream)
                     )
        }
    }

    if(fMin > fMax)
    {
        double f = fMax;
        fMax = fMin;
        fMin = f;
        LOG_WARN( _log
                , "Line %u: Plot info: Lower frequency boundary is greater than higher"
                  " boundary"
                , tok_getLine(_hTokenStream)
                );
    }
    if(fMin == fMax)
    {
        noPoints = 1;
        LOG_WARN( _log
                , "Line %u: Plot info: Frequency boundaries are identical. Only a single"
                  " frequency point can be plotted"
                , tok_getLine(_hTokenStream)
                );
    }
    else 
    {
        /* For logarithmic x axes the number of points is meant as number per decade. */
        assert(fMax/fMin > 1.0);
        if(isLogX)
            noPoints = (unsigned int)((double)noPoints * log10(fMax/fMin) + 0.5);

        if(noPoints < 2)
        {
            noPoints = 2;
            LOG_WARN( _log
                    , "Line %u: Plot info: Two frequency points should be plotted at least"
                    , tok_getLine(_hTokenStream)
                    );
        }
    }    
        
    pci_plotInfo_t *pPlotInfo;
    if(!_parseError)
    {
        pPlotInfo = smalloc(sizeof(pci_plotInfo_t), __FILE__, __LINE__);
        pPlotInfo->isLogX   = isLogX;
        pPlotInfo->noPoints = noPoints;
        pPlotInfo->freqLimitAry[0] = fMin;
        pPlotInfo->freqLimitAry[1] = fMax;
    }
    else
        pPlotInfo = NULL;

    return pPlotInfo;

} /* End of parsePlotInfo */




/**
 * Parse an identifier, which is basically a function of the underlaying tokenizer; this
 * functiions adds some general error handling.
 *   @return
 * Get the malloc allocated string holding the parsed identifier - or NULL if a problem
 * appeared. If so, all appropriate information has been written to the log. The function
 * caller becomes the owner of the returned string and needs to do a free after use.
 *   @param meaningOfIdent
 * A short string that indicates the meaning of the parsed indentifier. The string makes
 * the error messages more meaningful. Should start with a capital letter.
 *   @remark
 * The global error flag \a _parseError is set and a message is written to the log in case
 * of an error.
 */

static const char *parseIdentifier(const char * const meaningOfIdent)
{
    const char *ident;

    if(_token.type == tok_tokenTypeIdentifier)
    {
        /* Take ownership of malloc allocated identifier string. */
        ident = _token.value.identifier;
        _token.value.identifier = NULL;
        getToken();
    }
    else
    {
        _parseError = true;
        ident = NULL;
        LOG_ERROR(_log, "Line %u: Expect identifier: %s", tok_getLine(_hTokenStream), meaningOfIdent)
    }

    return ident;

} /* End of parseIdentifier */




/**
 * Parse a line, which defines a voltage of interest as the difference of two nodes'
 * electric potentials.
 *   @return
 * The function returns true if the definition could be parsed error free. If it returns
 * false than all according error messages and hints have been written to the log file.
 *   @param pParseResult
 * The parse result is placed in \a * pParseResult.
 *   @remark
 * The global error flag \a _parseError is set and a message is written to the log in case
 * of an error.
 */

static boolean parseVoltageDefintion(pci_circuit_t * const pParseResult)
{
    assert(_parseError == false);

    if(pParseResult->noVoltageDefs >= PCI_MAX_NO_VOLTAGE_DEFINITIONS)
    {
        _parseError = true;
        LOG_FATAL( _log
                 , "Line %u: Maximum number of %u voltage definitions exceeded"
                 , tok_getLine(_hTokenStream)
                 , PCI_MAX_NO_VOLTAGE_DEFINITIONS
                 );
        return false;
    }

    /* Create new voltage definition array entry. */
    pci_voltageDef_t * const pVolDef = &pParseResult
                                        ->voltageDefAry[pParseResult->noVoltageDefs];

    /* Read the name of the user-defined voltage. */
    pVolDef->name = parseIdentifier(/* meaningOfIdent */ "Name of user-defined voltage");

    /* Double-check, that the name is not yet in use. The voltages have their own name
       space. Clashes with device, node or result names are not recognized. They are not
       harmful but may make it more difficult to understand results and error feedback. */
    unsigned int u;
    for(u=0; u<pParseResult->noVoltageDefs; ++u)
    {
        if(strcmp(pParseResult->voltageDefAry[u].name, pVolDef->name) == 0)
        {
            _parseError = true;
            LOG_ERROR( _log
                     , "Line %u: Voltage name %s had been used before. Voltage names need"
                       " to be unique"
                     , tok_getLine(_hTokenStream)
                     , pVolDef->name
                     )
            break;
        }
    }
            
    /* Parse plus and minus node, which define the voltage. */
    if(!_parseError)
    {
        unsigned int idxNodeAry[2];
        if(parseListOfNodes(pParseResult, idxNodeAry, /* noNodes */ 2))
        {
            pVolDef->idxNodePlus  = idxNodeAry[0];
            pVolDef->idxNodeMinus = idxNodeAry[1];
        }
        else
        {
            LOG_ERROR( _log
                     , "Line %u: Definition of voltage %s requires the specification"
                       " of two nodes. The output voltage is undefined"
                     , tok_getLine(_hTokenStream)
                     , pVolDef->name
                     )
        }
    }

    if(_parseError)
    {
        /* The name string might have been allocated already; free it if not used. */
        if(pVolDef->name != NULL)
            free((char*)pVolDef->name);

        return false;
    }
    else
    {
        /* Everything is okay, release array element as new voltage definition. */
        ++ pParseResult->noVoltageDefs;

        return true;
    }
} /* End of parseVoltageDefintion */




/**
 * Parse a line, which defines a user wanted result.
 *   @return
 * The function returns true if the definition could be parsed error free. If it returns
 * false than all according error messages and hints have been written to the log file.
 *   @param pParseResult
 * The parse result is placed in \a * pParseResult.
 *   @param isFullResult
 * Full result and Bode plot are both handled by this function. Pass, which keyword had
 * been seen, which of the two cases should be handled.
 *   @remark
 * The global error flag \a _parseError is set and a message is written to the log in case
 * of an error.
 */

static boolean parseResultDefintion(pci_circuit_t * const pParseResult, boolean isFullResult)
{
    assert(_parseError == false);

    if(pParseResult->noResultDefs >= PCI_MAX_NO_RESULTS)
    {
        _parseError = true;
        LOG_FATAL( _log
                 , "Line %u: Maximum number of %u result definitions exceeded"
                 , tok_getLine(_hTokenStream)
                 , PCI_MAX_NO_RESULTS
                 );
        return false;
    }

    /* Open a new result definition array entry. */
    pci_resultDef_t * const pResultDef = &pParseResult
                                          ->resultDefAry[pParseResult->noResultDefs];
    pResultDef->noDependents = 0;
    pResultDef->independentName = NULL;
    pResultDef->pPlotInfo = NULL;

    /* Read the name of the user demanded result. */
    pResultDef->name = parseIdentifier(/* meaningOfIdent */ "Name of user demanded result");

    /* Double-check, that the name is not yet in use. The results have their own name
       space. Clashes with device, node or voltages names are not recognized. They are not
       harmful but may make it more difficult to understand results and error feedback. */
    unsigned int u;
    for(u=0; u<pParseResult->noResultDefs; ++u)
    {
        if(strcmp(pParseResult->resultDefAry[u].name, pResultDef->name) == 0)
        {
            _parseError = true;
            LOG_ERROR( _log
                     , "Line %u: Result name %s had been used before. Result names need"
                       " to be unique"
                     , tok_getLine(_hTokenStream)
                     , pResultDef->name
                     )
            break;
        }
    }
            
    /* A series of identifiers follows up. For Bode plots we want to see exactely two, the
       dependent followed by the independent quantity. A full result only has dependet
       quantities, but number of those. What we parse are meaningless identifiers, but we
       store them context dependent. */
    if(isFullResult)
    {
        if(!_parseError)
        {
            /* Having no independent quantity is the indication of a full result. */
            assert(pResultDef->independentName == NULL);

            /* Parse a list of dependent quantities. */
            do
            {
                const char * const dependent = parseIdentifier
                                                    ( /* meaningOfIdent */
                                                      "Name of dependent quantity or"
                                                      " unknown"
                                                    );
                if(pResultDef->noDependents < PCI_MAX_SIZE_OF_RESULT_SETS)
                    pResultDef->dependentNameAry[pResultDef->noDependents++] = dependent;
                else
                {
                    _parseError = true;
                    LOG_FATAL( _log
                             , "Line %u: Maximum number of %u dependent quantities in a"
                               " result exceeded"
                             , tok_getLine(_hTokenStream)
                             , PCI_MAX_SIZE_OF_RESULT_SETS
                             );
                }
            }
            while(!_parseError &&  _token.type == tok_tokenTypeIdentifier);

        } /* End if(No error yet?) */
    }
    else
    {
        /* Parse the dependent quantity. */
        if(!_parseError)
        {
            pResultDef->dependentNameAry[pResultDef->noDependents++] =
                                parseIdentifier( /* meaningOfIdent */
                                                 "Name of dependent quantity (behind the"
                                                 " result's name)"
                                               );
        } /* End if(No error yet?) */

        /* Parse the independent quantity. */
        if(!_parseError)
        {
            pResultDef->independentName = parseIdentifier( /* meaningOfIdent */
                                                           "Name of independent quantity"
                                                           " (behind result name and"
                                                           " dependent quantity)"
                                                         );
        } /* End if(No error yet?) */

    } /* End if(Full result or Bode plot?) */

    /* The plot supporting information is optional. */
    if(!_parseError
       &&  (_token.type == tokenTypePlotLinAxis  ||  _token.type == tokenTypePlotLogAxis)
      )
    {
        pResultDef->pPlotInfo = parsePlotInfo();
    }


    if(_parseError)
    {
        /* The name string might have been allocated already; free it if parse result is
           not usable. */
        if(pResultDef->name != NULL)
            free((char*)pResultDef->name);

        /* Free all already allocated names of (in)dependents. */
        while(pResultDef->noDependents > 0)
        {
            char *pMallocString =
                            (char*)pResultDef->dependentNameAry[--pResultDef->noDependents];

            /* If parsing the name caused the parse error then we can have a NULL pointer as
               entry in the array. */
            if(pMallocString != NULL)
                free((char*)pMallocString);
        }

        if(pResultDef->independentName != NULL)
        {
            free((char*)pResultDef->independentName);
            pResultDef->independentName = NULL;
        }

        assert(pResultDef->pPlotInfo == NULL);

        return false;
    }
    else
    {
        /* Everything is okay, release array element as new result definition. */
        ++ pParseResult->noResultDefs;

        return true;
    }
} /* End of parseResultDefintion */




/**
 * Validate that a node is a true, physical network node, i.e. that it really is connected
 * to an electrical connector of a device (but not only to a voltage sense input or not at
 * all).
 *   @return
 * The function returns \a false and emitts a message if a problem is found. Otherwise it
 * returns \a true;
 *   @param pParseResult
 * The completed parse result, which is under validation.
 *   @param nodeIdx
 * The node index to validate.
 */ 

static boolean checkNodeReference( const pci_circuit_t * const pParseResult
                                 , unsigned int nodeIdx
                                 )
{
    assert(nodeIdx < pParseResult->noNodes);
    
    /* A node is valid if it is connected to at least a single device. The control
       inputs of voltage controlled sources don't count. */
    unsigned int idxDev;
    for(idxDev=0; idxDev<pParseResult->noDevices; ++idxDev)
    {
        const pci_device_t * const pDev = pParseResult->pDeviceAry[idxDev];
        
        /* Unused nodes are set to PCI_NULL_NODE and are surely not accidentally identical
           with the node index to validate. */
        assert(pDev->idxNodeFrom != PCI_NULL_NODE  &&  pDev->idxNodeTo != PCI_NULL_NODE
               && (pDev->idxNodeOpOut == PCI_NULL_NODE  ||  pDev->type == pci_devType_opAmp)
              );
        if(nodeIdx == pDev->idxNodeFrom  ||  nodeIdx == pDev->idxNodeTo 
           ||  nodeIdx == pDev->idxNodeOpOut
          )
        {
            return true;
        }
    }

    _parseError = true;
    LOG_ERROR( _log
             , "Node %s is referenced for voltage sensing or by a voltage definition"
               " but it is not a true network node. Only true network nodes can be"
               " referenced for voltage sensing or by user-defined voltages; such"
               " nodes are connected to at least one device"
             , pParseResult->nodeNameAry[nodeIdx]
             )
    return false;

} /* End of checkNodeReference */




/**
 * The definition of the system output using tag U2 in the elder format and the voltage
 * sensing inputs of a voltage controlled source support forward references (as most
 * existing circuit files tend to place the tags U1 and U2 at the beginning). The character
 * of a forward reference is that it might be not satisfied at the end of parsing.\n
 *   This function should be called after parsing has been completed in order to find out if
 * the nodes referenced by voltage controlled sources or the system output definition of
 * the elder format are valid nodes of the circuit.
 *   @return
 * The function returns \a false and emitts a message if a problem is found. Otherwise it
 * returns \a true;
 *   @param pParseResult
 * The completed parse result.
 */ 

static boolean checkNodeReferences(const pci_circuit_t * const pParseResult)
{
    /* The validation is sensible only after successful parsing and we use the global
       variable for the return value of this function. */
    assert(!_parseError);
    
    /* We find all node references if we filter the list of devices for voltage controlled
       sources and if we then process all user-defined voltages. */
    unsigned int u;
    for(u=0; u<pParseResult->noDevices; ++u)
    {
        const pci_device_t * const pDev = pParseResult->pDeviceAry[u];
        if(pDev->type == pci_devType_srcUByU  ||  pDev->type == pci_devType_srcIByU)
        {
            /* We do not stop after the first failing validation in order to have best user
               feedback. */
            boolean success = checkNodeReference(pParseResult, pDev->idxNodeCtrlPlus);
            if(!checkNodeReference(pParseResult, pDev->idxNodeCtrlMinus))
                success = false;
                
            if(!success)
            {
                LOG_ERROR( _log
                         , "The control inputs of device %s (%s) reference invalid nodes."
                           " Please refer to previous messages for details"
                         , pDev->name
                         , pci_getNameOfDeviceType(pDev)
                         )
            }
        }
    } /* End for(All devices) */
                
    for(u=0; u<pParseResult->noVoltageDefs; ++u)
    {
        const pci_voltageDef_t * const pVolDef = &pParseResult->voltageDefAry[u];
        boolean success = checkNodeReference(pParseResult, pVolDef->idxNodePlus);
        if(!checkNodeReference(pParseResult, pVolDef->idxNodeMinus))
            success = false;

        if(!success)
        {
            assert(_isStdFormat || u == 0);
            LOG_ERROR( _log
                     , "The %s %s has invalid node references."
                       " Please refer to previous messages for details"
                     , _isStdFormat? "user-defined voltage": "system output definition"
                     , pVolDef->name
                     )
        }
    } /* End for(All user defined voltages) */

    return !_parseError;
    
} /* End of checkNodeReferences */




/**
 * Initialize the module at application startup.
 *   @remark
 * Do not forget to call the counterpart at application end.
 *   @remark Using this function is not an option but a must. You need to call it
 * prior to any other call of this module and prior to accessing any of its global data
 * objects.
 *   @see void pci_shutdownModule()
 */

void pci_initModule()
{
#ifdef  DEBUG
    /* The DEBUG compilation counts all references to all created objects. */
    _noRefsToObjects = 0;
#endif

#ifdef  DEBUG
    /* Check if patch of snprintf is either not required or properly installed. */
    char buf[3] = {[2] = '\0'};
    snprintf(buf, 2, "%s World", "Hello");
    assert(strlen(buf) == 1);
#endif
} /* End of pci_initModule */




/**
 * Do all cleanup after use of the module, which is required to avoid memory leaks, orphaned
 * handles, etc.
 */

void pci_shutdownModule()
{
#ifdef  DEBUG
    /* The DEBUG compilation looks for still allocated objects in order to detect memory
       leaks. */
    if(_noRefsToObjects != 0)
    {
        fprintf( stderr
               , "pci_shutdownModule: %u references to objects of type"
                 " pci_circuit_t have not been discarded at application"
                 " shutdown. There are probable memory leaks\n"
               , _noRefsToObjects
               );
    }
#endif
} /* End of pci_shutdownModule */




/**
 * Parse the input file.
 *   @return
 * The function returns true if the file could be parsed entirely error free. If it returns
 * false than all according error messages and hints have been written to the log file.
 *   @param hLogger
 * The handle to an opened and configured logger is passed to the function. All progressa
 * and result messages are written to the logger.
 *   @param ppParseResult
 * The pointer to the parse result object is placed in \a * ppParseResult. After usage the
 * reference to the data structure needs to be released with \a pci_deleteParseResult.\n
 *   The returned pointer is valid only if the function returns true.
 *   @param inputFileName
 * The name of the circuit file to parse.
 *   @see const pci_circuit_t *pci_cloneByConstReference(const pci_circuit_t * const)
 *   @see void pci_deleteParseResult(const pci_deviceRelation_t *pParseResult)
 */

boolean pci_parseCircuitFile( log_hLogger_t hLogger
                            , const pci_circuit_t * * const ppParseResult
                            , const char * const inputFileName
                            )
{
    boolean parseError = false;

    /* Use the passed logger during the parsing process. */
    _log = hLogger;

    /* Set the format. The decision is based on the file name: an extension .ckt is
       associated with the old format. */
    size_t lenName = strlen(inputFileName);
    _isStdFormat = lenName < 4  || stricmp(&inputFileName[lenName-4], ".ckt") != 0;

    /* The elder format used to be case insensitive. Set stricmp as global string compare
       function. */
    if(_isStdFormat)
        _strcmp = strcmp;
    else
        _strcmp = stricmp;

    if(!parseError && !openInput(inputFileName, /* open */ true))
        parseError = true;
    else
    {
        LOG_INFO(_log, "Start reading circuit file %s", inputFileName);

        /* Place first lexical atom into the global variable. */
        if(!getToken())
            parseError = true;
    }

    pci_circuit_t *pParseResult = NULL;
    pci_plotInfo_t *pPlotInfoOld = NULL;
    if(!parseError)
    {
        /* Create result structure. */
        pParseResult = createParseResult();

        /* Start counting the tags "U1" in the old format. */
        _noOldStyleInputDefs = 0;

        /* Read and interpret all lexical tokens read from the input until token
           end-of-file is seen. */
        do
        {
            /* Clear the global error flag. We want to parse the next input line evn if we
               saw some errors before. */
            _parseError = false;

            /* Trivial case: Token end-of-file. This line is done. */
            boolean recognized = _token.type == EOF;

            /* The most simple kind of line: a blank line. */
            while(!recognized && _token.type == tok_tokenTypeEndOfLine)
            {
                recognized = true;
                getToken();
            }


            /* The old format allowed to specify some plot support information in a
               dedicated line (.AC ...). */
            if(!recognized && !_isStdFormat &&  _token.type == tokenTypePlotInfo)
            {
                recognized = true;
                if(pPlotInfoOld == NULL)
                    pPlotInfoOld = parsePlotInfo();
                else
                {
                    _parseError = true;
                    LOG_ERROR( _log
                             , "Line %u: Plot information is repeatedly specified."
                               " .AC must be used only once"
                             , tok_getLine(_hTokenStream)
                             )
                }
            }

            /* Most frequent case: A specific, reserved identifier defines the beginning of
               a device or a network edge definition. */
            if(!recognized && _token.type == tok_tokenTypeIdentifier)
            {
                assert(_token.value.identifier != NULL);
                if((_isStdFormat
                    && ((strlen(_token.value.identifier) == 1
                         &&  strchr("RYCLUI", _token.value.identifier[0]) != NULL
                        )
                        ||  _strcmp(_token.value.identifier, "OP") == 0
                        ||  _strcmp(_token.value.identifier, "PI") == 0
                       )
                   )
                   || (!_isStdFormat
                       && ((strlen(_token.value.identifier) >= 1
                            &&  strchr("RYCLGS", toupper(_token.value.identifier[0])) != NULL
                           )
                           ||  _strcmp(_token.value.identifier, "OP") == 0
                           ||  _strcmp(_token.value.identifier, "U1") == 0
                          )
                      )
                  )
                {
                    /* A device definition follows. */
                    recognized = true;
                    parseDeviceDef(pParseResult);
                }
            } /* End if(Device definition?) */


            /* The output definition differs in the old format and is here very similar
               to a device definition. */
            if(!recognized && !_isStdFormat
               && _token.type == tok_tokenTypeIdentifier
               && _strcmp(_token.value.identifier, "U2") == 0
              )
            {
                recognized = true;

                getToken();
                parseOldStyleOutput(pParseResult);
            }


            /* The standard format has a dedicated line to specify a voltage of interest as a
               difference of two node voltages. */
            if(!recognized && _isStdFormat
               &&  _token.type == tokenTypeVoltageDef
              )
            {
                recognized = true;

                getToken();
                parseVoltageDefintion(pParseResult);
            }


            /* The standard format has a dedicated line to specify a result. This includes a
               full result or a Bode plot. */
            if(!recognized && _isStdFormat
               &&  (_token.type == tokenTypeResultDef || _token.type == tokenTypeBodeResultDef)
              )
            {
                recognized = true;

                const boolean isFullResult = _token.type == tokenTypeResultDef;
                getToken();
                parseResultDefintion(pParseResult, isFullResult);
            }


            /* A comment line, defined by a leading asterisk. Only defined in the old
               format. */
            if(!recognized && !_isStdFormat && _token.type == '*')
            {
                recognized = true;

                /* Advance the token stream until the end of the current line. Ignore any
                   errors: we are in a comment not in a piece of formal syntax. */
                do
                {
                    tok_getNextToken(_hTokenStream, &_token);
                }
                while(_token.type != tok_tokenTypeEndOfLine
                      &&  _token.type != tok_tokenTypeEndOfFile
                     );

                /* ... not until but beyond. In this step we must not ignore an error any
                   longer. */
                if(_token.type == tok_tokenTypeEndOfLine)
                    getToken();
            }


            /* It's a syntax error if we didn't recognize the token at all. */
            if(!recognized)
            {
                _parseError = true;
                LOG_ERROR( _log
                         , "Line %u: Syntax error. Expect a device definition or plot"
                           " information"
                         , tok_getLine(_hTokenStream)
                         )
            }


            /* Reading the current line might have ended in a parse error, which would be
               recognized by the set global error flag. If so, we look for the beginning of
               the next line before continuing parsing. This way, we can proceed parsing,
               and maybe find more problems in this same run. */
            if(_parseError)
            {
                parseError = _parseError;
                sync();
            }
        }
        while(_token.type != EOF);

    } /* End if(Input file could be opened) */

    /* Validate all forward references to nodes now that we know all nodes. */
    if(!parseError && !checkNodeReferences(pParseResult))
        parseError = true;

    /* The elder format demands to define a single input voltage and this is implicitly
       part of the only result definition. Double-check the found number. */
    if(!_isStdFormat && !parseError &&  _noOldStyleInputDefs != 1)
    {
        parseError = true;
        LOG_ERROR( _log
                 , _noOldStyleInputDefs > 1
                   ? "Line %u: %u input voltages have been defined. Please note that only"
                     " a single system input can be defined"
                   : "Line %u: No input voltage has been defined. Please consider"
                     " using lines U1 and U2 to define system in- and output"
                 , tok_getLine(_hTokenStream)
                 , _noOldStyleInputDefs
                 )
    }

    /* The elder format permits to define a single output voltage and this is implicitly
       the only result definition. Associate a possibly read plot information with this one
       and only result. */
    if(!_isStdFormat && !parseError)
    {
        assert(pParseResult->noResultDefs == 0);
        if(pParseResult->noVoltageDefs == 1)
        {
            /* Demand a plot result for the input impedance Z = U1(I_U1). */
            pci_resultDef_t *pResDef = &pParseResult->resultDefAry[pParseResult->noResultDefs];

            pResDef->name = stralloccpy("Z");
            pResDef->noDependents = 1;
            pResDef->dependentNameAry[0] = stralloccpy("U1"); /* U1 is a known in any *.ckt */
            pResDef->independentName = stralloccpy("I_U1"); /* I_U1 is internally derived */
            pResDef->pPlotInfo = pPlotInfoOld;              /* from U1 */
            ++ pParseResult->noResultDefs;

            /* Demand a plot result for the one and only output voltage. It is shown as
               function of the one and only input voltage: G = U2(U1). */
            ++ pResDef;
            pResDef->name = stralloccpy("G");
            pResDef->noDependents = 1;
            pResDef->dependentNameAry[0] = stralloccpy("U2");
            pResDef->independentName = stralloccpy("U1"); /* U1 is a known in a valid *.ckt */

            /* Each result has its own plot information. Make a deep copy of the object if
               it is present. */
            if(pPlotInfoOld != NULL)
            {
                pResDef->pPlotInfo = smalloc(sizeof(pci_plotInfo_t), __FILE__, __LINE__);
                *pResDef->pPlotInfo = *pPlotInfoOld;
            }
            else
                pResDef->pPlotInfo = NULL;

            ++ pParseResult->noResultDefs;

            pPlotInfoOld = NULL;
        }
        else
        {
            assert(pParseResult->noVoltageDefs == 0);

            parseError = true;
            LOG_ERROR( _log
                     , "Line %u: No output voltage has been defined. Please consider"
                       " using lines U1 and U2 to define system in- and output"
                     , tok_getLine(_hTokenStream)
                     )
        }
    }

    /* In case of parse errors there might be an orphaned plot info object. */
    if(pPlotInfoOld != NULL)
        free(pPlotInfoOld);

    /* Try to close input file, even in case of failures. */
    openInput(inputFileName, /* open */ false);

    if(parseError)
        LOG_ERROR(_log, "Reading circuit file %s failed", inputFileName)
    else
        LOG_RESULT(_log, "Reading circuit file %s successfully done", inputFileName)

    /* Invalidate the reference to the passed logger. After return it must no longer be
       used. */
    _log = LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT;

    if(parseError)
    {
        pci_deleteParseResult(pParseResult);
        *ppParseResult = NULL;
    }
    else
        *ppParseResult = pParseResult;

    return !parseError;

} /* End of pci_parseCircuitFile */




/**
 * Request a reference to a parse result object. The new reference is counted internally for
 * later and safe control of the delete operation.\n
 *   Any requested reference needs to be freed with pci_deleteParseResult after use.
 *   @return
 * A copy of the passed pointer \a pParseResult is returned.
 *   @param pParseResult
 * Pointer to the object to be cloned by reference. The object has originally been created
 * by a successful call of pci_parseCircuitFile, but another copied reference got from this
 * function may also be passed.
 *   @see boolean pci_parseCircuitFile(log_hLogger_t, const pci_circuit_t * * const, const
 * char * const)
 *   @see void pci_deleteParseResult(const pci_circuit_t *)
 */

const pci_circuit_t *pci_cloneByConstReference(const pci_circuit_t * const pParseResult)
{
    if(pParseResult == NULL)
        return NULL;

    /* Type cast: The parse result objects are actually read only to the client and should
       be used as such by the client. Therefore they are declared const in the public
       interface. */
    ++ ((pci_circuit_t*)pParseResult)->noReferencesToThis;

#ifdef DEBUG
    ++ _noRefsToObjects;
#endif

    return pParseResult;

} /* End of pci_cloneByConstReference */






/**
 * Delete a reference to a parse result object after use. If there are no references left
 * then the object itself is deleted, all memory is freed.
 *   @param pParseResult
 * * \a pParseResult is the unchanged parse result as got from pci_parseCircuitFile before.
 *   @see boolean pci_parseCircuitFile(log_hLogger_t, const pci_circuit_t * * const, const
 * char * const)
 *   @see const pci_circuit_t *pci_cloneByConstReference(const pci_circuit_t * const)
 */

void pci_deleteParseResult(const pci_circuit_t *pParseResult)
{
    /* Hint: The global logger is not available for the implementation of this method.*/

    if(pParseResult == NULL)
        return;

    /* Deletion takes place only if there are no known other references to this object. */
    assert(pParseResult->noReferencesToThis >= 1);
    if(--((pci_circuit_t*)pParseResult)->noReferencesToThis == 0)
    {
        /* Tpye casts to void*: They permit freeing but keep the caller in state to operate
           on a const parse object. */
        unsigned int u;
        for(u=0; u<pParseResult->noNodes; ++u)
            free((void*)pParseResult->nodeNameAry[u]);
        for(u=0; u<pParseResult->noDevices; ++u)
        {
            free((char*)pParseResult->pDeviceAry[u]->name);
            free((void*)pParseResult->pDeviceAry[u]);
        }
        for(u=0; u<pParseResult->noVoltageDefs; ++u)
            free((char*)pParseResult->voltageDefAry[u].name);

        for(u=0; u<pParseResult->noResultDefs; ++u)
        {
            const pci_resultDef_t *pResDef = &pParseResult->resultDefAry[u];

            free((char*)pResDef->name);

            unsigned int v;
            for(v=0; v<pResDef->noDependents; ++v)
            {
                assert(pResDef->dependentNameAry[v] != NULL);
                free((char*)pResDef->dependentNameAry[v]);
            }
            if(pResDef->independentName != NULL)
                free((char*)pResDef->independentName);
            if(pResDef->pPlotInfo != NULL)
                free((void*)pResDef->pPlotInfo);
        }

        free((void*)pParseResult);

    } /* End if(Object but not only the reference to it is deleted) */

#ifdef DEBUG
    -- _noRefsToObjects;
#endif
} /* End of pci_deleteParseResult */




/**
 * Query a readable name for the type of a device.
 *   @return
 * Get the type name as a pointer to a constant read-only character string.
 *   @param pDevice
 * The pointer to the device to query the type name for.
 */

const char *pci_getNameOfDeviceType(const pci_device_t * const pDevice)
{
    return deviceTypeToString(pDevice->type);

} /* End of pci_getNameOfDeviceType */




/**
 * Render a plot information object as Octave script code. The object is represented as a M
 * code struct; the generated M code can e.g. be used as RHS of an assignment.
 *   @param pPlotInfo
 * The reference to the rendered plot information object is passed. NULL may be passed if
 * no specific plot information is known and plot attributes are modelled as "don't care".
 *   @param pMScript
 * The pointer to an M script object. The generated M code is written into this M script.
 * The object is the result of a successful call of boolean msc_createMScript(msc_mScript_t
 * ** const, const char * const, const char * const, const char * const).
 *   @param indentStr
 * The generated code starts with a new line. This and each other required line will begin
 * with the passed string.
 */

void pci_exportPlotInfoAsMCode( msc_mScript_t * const pMScript
                              , const pci_plotInfo_t * const pPlotInfo
                              , const char *indentStr
                              )
{
    /* Write into the stream associated with the M script object. */
    FILE *hMFile = msc_borrowStream(pMScript);
    
    if(pPlotInfo != NULL)
    {
        double fMin, fMax;
        if(pPlotInfo->freqLimitAry[1] >= pPlotInfo->freqLimitAry[0])
        {
            fMin = pPlotInfo->freqLimitAry[0];
            fMax = pPlotInfo->freqLimitAry[1];
        }
        else
        {
            fMin = pPlotInfo->freqLimitAry[1];
            fMax = pPlotInfo->freqLimitAry[0];
        }
        
        unsigned long noPoints = pPlotInfo->noPoints;
        if(fMin < fMax)
        {
            if(noPoints < 2)
                noPoints = 2;
        }
        else if(fMin == fMax)
            noPoints = 1;

        fprintf( hMFile
               , "%sstruct( 'isLogX', %s ...\n"
                 "%s      , 'noPoints', %lu ...\n"
                 "%s      , 'freqMin', %.6g ... %% Hz\n"
                 "%s      , 'freqMax', %.6g ... %% Hz\n"
                 "%s      )"
               , indentStr
               , pPlotInfo->isLogX? "true": "false"
               , indentStr
               , noPoints
               , indentStr
               , fMin
               , indentStr
               , fMax
               , indentStr
               );
    }               
    else
    {
        /* An empty struct having the correct structure is generated to indicate "use
           plotting defaults". */
        fprintf( hMFile
               , "%sstruct( 'isLogX', {} ...\n"
                 "%s      , 'noPoints', {} ...\n"
                 "%s      , 'freqMin', {} ...\n"
                 "%s      , 'freqMax', {} ...\n"
                 "%s      )"
               , indentStr
               , indentStr
               , indentStr
               , indentStr
               , indentStr
               );
    }               
    
    /* The probable final semicolon and a newline depends on the application of the
       generated M code is in the responsibility of the caller. */
        
    msc_releaseStream(pMScript);
    
} /* End of pci_exportPlotInfoAsMCode */




