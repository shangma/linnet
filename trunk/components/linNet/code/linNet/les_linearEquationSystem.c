/**
 * @file les_linearEquationSystem.c
 *   Analyse the parsed network structure and create the linear equation system.
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
 *   les_initModule
 *   les_shutdownModule
 *   les_createLES
 *   les_deleteLES
 *   les_getNoVariables
 *   les_getTableOfVariables
 *   les_setupLES
 * Local functions
 *   lookForSubNetByNode
 *   concatNodeLists
 *   logNetworkTopology
 *   analyseNetworkTopology
 *   deleteNetwork
 *   findNodeGnd
 *   createNameOfUnknown
 *   determineReqVariables
 *   addSrcUConditions
 *   addSrcIConditions
 *   addPassiveDeviceConditions
 *   addOpAmpConditions
 *   addCurrentProbeConditions
 *   addSrcUByUConditions
 *   addSrcUByIConditions
 *   addSrcIByUConditions
 *   addSrcIByIConditions
 *   addDeviceConditions
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
#include "pci_parserCircuit.h"
#include "tbv_tableOfVariables.h"
#include "coe_coefficient.h"
#include "coe_coefficient.inlineInterface.h"
#include "les_linearEquationSystem.h"


/*
 * Defines
 */

/** Symbols introduced by the internal algorithms are derived from known, user defined
    names by adding a pre- or suffix. The required buffer size for the derived names is the
    length of the base name plus the value of this define. */
#define MAX_SIZE_OF_SYMBOL_PREFIX    (10+1)


/** The prefix used to derive the name of an unknown voltage from a node's name.\n
      Truncation of names is safely avoided if the prefix defined here is no longer
    than #MAX_SIZE_OF_SYMBOL_PREFIX-1. */
#define SYMBOL_PREFIX_VOLTAGE   "U_"

/** The prefix used to derive the name of an unknown current from a device name.\n
      Truncation of names is safely avoided if the prefix defined here is no longer
    than #MAX_SIZE_OF_SYMBOL_PREFIX-1. */
#define SYMBOL_PREFIX_CURRENT   "I_"


/*
 * Local type definitions
 */

/** Part of the internal network representation: An element of the linked list of network
    nodes. */
typedef struct networkNodeRef_t
{
    /** The list is linked by this pointer and ends with a NULL. */
    struct networkNodeRef_t *pNext;

    /** The reference to the node (by index into the net list representing the circuit). */
    unsigned int idxNode;

    /** A specific check of the device interconnection: No two op-amps must be connected to
        the same node. This flags is used to recognize this problem. */
    boolean isOpampOutput;

#ifdef DEBUG
    /** Protection against misuse. */
    boolean isInUse;
#endif
} networkNodeRef_t;


/** Part of the internal network representation: An element of the linked list of
    unconnected (sub-)networks. */
typedef struct subNetwork_t
{
    /** The list is linked by this pointer and ends with a NULL. */
    struct subNetwork_t *pNext;

    /** A list of nodes, which belong to this connected sub-part of entire network graph.
        Or NULL if none. */
    networkNodeRef_t *pHeadOfNodeList;

} subNetwork_t;


/** The representation of the complete network. */
typedef struct
{
    /** The number of sub-networks. */
    unsigned int noSubNets;

    /** The list of sub-networks or sub-graphs. */
    subNetwork_t *pHeadOfSubNetList;

    /** The network node reference objects are stored in a linear array. This way, they can
        be directly used as lookup table from node index (as defined by pci_circuit_t) to
        the additional node properties defined in the reference object. */
    networkNodeRef_t *nodeRefAry;

    /** Used for user feedback only: If a circuit has at least one controlled source, that
        unconnected sub-graphs are less suspicious and no warning is emitted. */
    boolean hasControlledSources;

    /** The entries in this data structure refer to a parse result, that describes all
        nodes and devices in the circuit. A copy (by reference) of this net list
        representing the circuit we refer to is part of this object. */
    const pci_circuit_t *pCircuitNetList;

} network_t;


/*
 * Local prototypes
 */

static void deleteNetwork(network_t * const pNetwork);


/*
 * Data definitions
 */

/** A global logger object is referenced from anywhere for writing progress messages. */
static log_hLogger_t _log = LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT;

/** A heap specific for objects of type subNetwork_t. */
mem_hHeap_t _hMemMgrSubNet = MEM_HANDLE_INVALID_HEAP;


/*
 * Function implementation
 */

/**
 * Iterate through a list of sub-networks and look for the first one, which contains a
 * given node.
 *   @return
 * A pointer to the sub-network that contains the node or NULL if no such sub-network exists.
 *   @param pNetwork
 * A pointer to the first network object in the linked list of such.
 *   @param idxNode
 * The node to look for is identified by index.
 *   @remark Throughout the entire run of the algorithm there will never be a node in more
 * than one sub-network's node list.
 */

static subNetwork_t *lookForSubNetByNode( const network_t * const pNetwork
                                        , unsigned int idxNode
                                        )
{
    subNetwork_t *pSubNet = pNetwork->pHeadOfSubNetList;
    while(pSubNet != NULL)
    {
        const networkNodeRef_t *pNode = pSubNet->pHeadOfNodeList;
        while(pNode != NULL)
        {
            /* We only look for the first match and may immediately return. */
            if(pNode->idxNode == idxNode)
                return pSubNet;

            /* Go to next node in the same network. */
            pNode = pNode->pNext;
        }

        /* Go to next network. */
        pSubNet = pSubNet->pNext;
    }

    /* All networks inspected, no one contains the node. */
    return NULL;

} /* End of lookForSubNetByNode */




/**
 * Combine two node lists to a single one.
 *   @param ppHeadOfMajorList
 * A pointer to the pointer variable that points to the head of the first node list. The
 * other list will be appended to this list and after return \a *ppHeadOfMajorList will point
 * to the combined list.
 *   @param ppHeadOfMinorList
 * A pointer to the pointer variable that points to the head of the second node list. This
 * list is transfered to the end of the major list and the pointer to the head of the
 * second list is set to NULL, i.e. \a *ppHeadOfMinorList = NULL. After return there won't
 * be doubled references to the elements of the minor list.
 */

static void concatNodeLists( networkNodeRef_t * * const ppHeadOfMajorList
                           , networkNodeRef_t * * const ppHeadOfMinorList
                           )
{
    if(*ppHeadOfMajorList == NULL)
    {
        /* Trivial case: Receiving list is still empty. Now just transfer the head pointer
           of the other list. */
        *ppHeadOfMajorList = *ppHeadOfMinorList;
    }
    else
    {
        /* Normal case: The other list is linked to the last element of the receiving list.
           We need to iterate through the receiving list in order to find its last
           element. */
        networkNodeRef_t *pNode = *ppHeadOfMajorList;
        while(pNode->pNext != NULL)
            pNode = pNode->pNext;

        /* pNode points to the last list element. Just overwrite the link pointer. */
        pNode->pNext = *ppHeadOfMinorList;
    }

    /* Avoid double references to same list elements: The minor list is emptied at the
       client side. */
    *ppHeadOfMinorList = NULL;

} /* End of concatNodeLists */




/**
 * Present the result of the network topology struture in the log. The user should find a
 * representation equivalent to the node list in his input circuit file.
 *   @param pNetwork
 * The network data structure to present.
 */

static void logNetworkTopology(const network_t * const pNetwork)
{
    LOG_DEBUG(_log, "Network topology after parsing input:");

    const subNetwork_t *pSubNet = pNetwork->pHeadOfSubNetList;
    unsigned int noSubNets = 0
               , noNodesTotal = 0;
    while(pSubNet != NULL)
    {
        ++ noSubNets;
        LOG_DEBUG(_log, "  Sub-network %u:", noSubNets);

        /* Iterate through all nodes of this sub-graph. */
        const networkNodeRef_t *pNode = pSubNet->pHeadOfNodeList;
        unsigned int noNodes = 0;
        while(pNode != NULL)
        {
            ++ noNodes;
            unsigned int idxNode = pNode->idxNode;
            LOG_DEBUG( _log
                     , "    Node %02u, %s"
                     , noNodes
                     , pNetwork->pCircuitNetList->nodeNameAry[idxNode]
                     );

            /* Go to the next node of the sub-graph. */
            pNode = pNode->pNext;

        } /* End while(All nodes in the sub-graph) */

        noNodesTotal += noNodes;

        /* Go to the next sub-graph. */
        pSubNet = pSubNet->pNext;

    } /* End while(All sub-graphs) */

    LOG_DEBUG( _log
             , "Network consists of %u unconnected graphs with a total of %u nodes"
             , noSubNets
             , noNodesTotal
             );

} /* End of logNetworkTopology */




/**
 * Iterate through the parsing result and find out how the nodes in use are interconnected.
 * Shape a new derived data structure, that represents the network in a way that permits to
 * setup the linear equation system.\n
 *   The returned object needs to be deleted after usage.
 *   @return
 * \a true if function succeeds. A network object has been created and no connectivity error
 * has been recognized. \a false otherwise.
 *   @param ppNetwork
 * The pointer to the built-up data structure, that represents the network structure is
 * returned in * \a ppNetwork.\n
 *   If the function returns false then * \a ppNetwork will contain NULL.
 *   @param pCircuitNetList
 * The net list as returned by the parser.
 *   @see void deleteNetwork(network_t * const)
 */

static boolean analyseNetworkTopology( network_t * * const ppNetwork
                                     , const pci_circuit_t * const pCircuitNetList
                                     )
{
    /* Targeted data structure: The built data structure is a list of unconnected
       sub-networks. (The input file may define n unconnected graphs of interconnected
       devices.) In the normal case, this list will have length one. Each sub-network is
       represented by a list of nodes, which are interconnected in this sub-network. */

    network_t *pNetwork = smalloc(sizeof(network_t), __FILE__, __LINE__);
    pNetwork->noSubNets = 0;
    pNetwork->pHeadOfSubNetList = NULL;
    pNetwork->nodeRefAry = smalloc( sizeof(networkNodeRef_t) * pCircuitNetList->noNodes
                                  , __FILE__
                                  , __LINE__
                                  );
    pNetwork->hasControlledSources = false;
#ifdef DEBUG
    unsigned int u
               , noNodeRefs = 0;
    for(u=0; u<pCircuitNetList->noNodes; ++u)
        pNetwork->nodeRefAry[u].isInUse = false;
#endif

    /* Make the net list representing the circuit, which the network_t members refer to,
       accessible from the network object. */
    pNetwork->pCircuitNetList = pci_cloneByConstReference(pCircuitNetList);

    /* Iterate over all devices and put the connected nodes into the current set. */
    unsigned int idxDev;
    for(idxDev=0; idxDev<pCircuitNetList->noDevices; ++idxDev)
    {
        const pci_device_t * const pDev = pCircuitNetList->pDeviceAry[idxDev];

        /* The operation is simplified by first putting all connectors of the
           device into a generic array. Then, we can later use a loop to do the rest. */
        struct
        {
            unsigned int idxNode;
            boolean isInterconnection;

        } nodeAry[4];
        unsigned int noNodes
                   , noOpAmps = 0;

        switch(pDev->type)
        {
        case pci_devType_resistor    :
        case pci_devType_conductance :
        case pci_devType_capacitor   :
        case pci_devType_inductivity :
        case pci_devType_srcU        :
        case pci_devType_srcI        :
        case pci_devType_currentProbe:
        case pci_devType_srcUByI     :
        case pci_devType_srcIByI     :
            nodeAry[0].idxNode = pDev->idxNodeFrom;
            nodeAry[1].idxNode = pDev->idxNodeTo;
            nodeAry[0].isInterconnection =
            nodeAry[1].isInterconnection = true;
            noNodes = 2;
            break;

        case pci_devType_opAmp:
            ++ noOpAmps;
            nodeAry[0].idxNode = pDev->idxNodeFrom;
            nodeAry[1].idxNode = pDev->idxNodeTo;
            nodeAry[2].idxNode = pDev->idxNodeOpOut;
            nodeAry[0].isInterconnection =
            nodeAry[1].isInterconnection = true;
            nodeAry[2].isInterconnection = false;
            noNodes = 3;
            break;

        case pci_devType_srcUByU:
        case pci_devType_srcIByU:
            nodeAry[0].idxNode = pDev->idxNodeFrom;
            nodeAry[1].idxNode = pDev->idxNodeTo;
            nodeAry[2].idxNode = pDev->idxNodeCtrlPlus;
            nodeAry[3].idxNode = pDev->idxNodeCtrlMinus;
            nodeAry[0].isInterconnection =
            nodeAry[1].isInterconnection = true;
            nodeAry[2].isInterconnection =
            nodeAry[3].isInterconnection = false;
            noNodes = 4;
            break;

        default:
            noNodes = 0;
            assert(false);
        }

        subNetwork_t *pJoinedSubNet = NULL;
        unsigned int u;
        for(u=0; u<noNodes; ++u)
        {
            /* Check if we put this node already into a sub-graph. */
            subNetwork_t *pSubNet = lookForSubNetByNode(pNetwork, nodeAry[u].idxNode);
            if(pSubNet == NULL)
            {
                /* This node has not been seen before, it constitutes a new sub-graph. */
                pSubNet = mem_malloc(_hMemMgrSubNet);
                pSubNet->pNext = NULL;
                pSubNet->pHeadOfNodeList = NULL;

                assert(nodeAry[u].idxNode < pCircuitNetList->noNodes);
                networkNodeRef_t * const pNodeRef = &pNetwork->nodeRefAry[nodeAry[u].idxNode];
#ifdef DEBUG
                assert(noNodeRefs < pCircuitNetList->noNodes  && !pNodeRef->isInUse);
                pNodeRef->isInUse = true;
                ++ noNodeRefs;
#endif
                pNodeRef->pNext = NULL;
                pNodeRef->idxNode = nodeAry[u].idxNode;
                pNodeRef->isOpampOutput = false;
                pSubNet->pHeadOfNodeList = pNodeRef;

                /* Now place the new sub-graph into the list of networks. */
                pSubNet->pNext = pNetwork->pHeadOfSubNetList;
                pNetwork->pHeadOfSubNetList = pSubNet;
                ++ pNetwork->noSubNets;

            } /* End if(Node is seen the first time?) */

            /* Here, in any case, pSubNet points to the sub-graph the node belongs to. If
               this is a different sub-graph and if it is an interconnecting node, then
               join this sub-graph with with those of the other interconnecting nodes of
               this device. */
            assert(pSubNet != NULL);
            if(nodeAry[u].isInterconnection)
            {
                if(pJoinedSubNet != NULL)
                {
                    if(pSubNet != pJoinedSubNet)
                    {
                        /* At the moment, joining the sub-graph just means to move the
                           nodes from the second sub-graph to the first one. The second
                           sub-graph continues to exist as node-less, empty graph. This
                           will be sorted out later. */
                        assert(pJoinedSubNet->pHeadOfNodeList != NULL
                               && pSubNet->pHeadOfNodeList != NULL
                              );
                        concatNodeLists( &pJoinedSubNet->pHeadOfNodeList
                                       , &pSubNet->pHeadOfNodeList
                                       );
                        assert(pJoinedSubNet->pHeadOfNodeList != NULL
                               && pSubNet->pHeadOfNodeList == NULL
                              );
                    }
                }
                else
                {
                    /* First loop iteration: Joining will only take place for the second,
                       third, etc node. */
                    pJoinedSubNet = pSubNet;
                }
            }
        } /* End for(All connected nodes) */

    } /* End for(All devices) */

    assert(noNodeRefs == pCircuitNetList->noNodes);


    /* Remove empty networks from the list. They are just an undesired artefact of the
       algorithm. */
    subNetwork_t *pSubNet = pNetwork->pHeadOfSubNetList
               , *pNewNotEmptyList = NULL;
    while(pSubNet != NULL)
    {
        subNetwork_t *pSubNetNext = pSubNet->pNext;

        if(pSubNet->pHeadOfNodeList != NULL)
        {
            /* Move list element pSubNet to to the output list. Actually, we copy a
               reference to this object. */
            pSubNet->pNext = pNewNotEmptyList;
            pNewNotEmptyList = pSubNet;
        }
        else
        {
            /* Update the number of sub-networks. */
            assert(pNetwork->noSubNets > 0);
            -- pNetwork->noSubNets;

            /* The no longer used empty network object can be freed. The reference to it
               still found in pNetwork->pHeadOfSubNetList will be deleted later. */
            mem_free(_hMemMgrSubNet, pSubNet);
        }

        /* Update pSubNet to continue iteration of input list. */
        pSubNet = pSubNetNext;
    }
    /* Replace the list of networks with the output list (of none empty networks only). All
       invalid references to meanwhile freed objects are deleted in this step. */
    boolean success = true;
    pNetwork->pHeadOfSubNetList = pNewNotEmptyList;
    if(pNetwork->noSubNets == 0)
    {
        success = false;
        LOG_ERROR(_log, "No network of interconnected devices found");
    }

    /* Unconnected sub-graphs can introduce some problems, which would lead to an
       unsolvable LES. Some, but not all of them, can be recognized right now. We do this
       as it improves the quality of user feedback. The following problems are anticipated:
         The outputs of different op-amps must not be connected to the same node.
         The output of an op-amp needs to belong to the same sub-graph as the inputs, as the
       active voltage potential equalization by current injection would be inhibited
       otherwise.
         The voltage sense inputs of a voltage controlled source need to belong to the same
       sub-graph, as the control voltage is undefined otherwise. For the same reason user
       defined voltages must not reference a pair of nodes from different sub-graphs.
         Note: Those problems caused by undefined voltage potential differences would not
       even be reported as an unsolvable LES as the further processing will select an
       appropriate ground node for each sub-graph. This actually defines a potential
       difference but not controlled by and transparent to the user. The result would be -
       although basically correct - incomprehensible, confusing and arbitrary to the user.
         Unconnected sub-graphs are physically reasonable and basically supported but they
       are useless at the same time and increase the computational complexity without need
       (as they could be modeled by distinct circuits). Therefore we'd like to emit a
       warning if such unconnected sub-graphs are found. Problem: Sub-graphs can be
       unconnected (with respect to current flow) but logically connected by a control
       input of a controlled source. The constellation is not fully figured out. Instead,
       we just remind if at least one controlled source is in use. If so, the message about
       unconnected graphs is emitted on informative level instead of warning level as
       usual.
         We loop over all devices and do the checks explicitly. Then we will loop over the
       user-defined voltages. */
    for(idxDev=0; idxDev<pCircuitNetList->noDevices; ++idxDev)
    {
        const pci_device_t * const pDev = pCircuitNetList->pDeviceAry[idxDev];

        switch(pDev->type)
        {
        case pci_devType_opAmp:
          {
            /* The ouputs of operational amplifiers must not be connected to a ground node.
               We mark this node persistently to support the later ground node selection.
                 At the same time we can double-check that no two op-amp outputs are
               connected to the same node. (Note: this is a necessary but not sufficient
               criterion for correct op-amp interconnectivity.) */
            assert(pDev->idxNodeOpOut < pCircuitNetList->noNodes);
            networkNodeRef_t *pNodeRef = &pNetwork->nodeRefAry[pDev->idxNodeOpOut];
            if(!pNodeRef->isOpampOutput)
                pNodeRef->isOpampOutput = true;
            else
            {
                success = false;
                LOG_ERROR( _log
                         , "Multiple op-amps are connected to the same network node"
                           " %s. The second one is named %s. Connecting op-amp outputs"
                           " leads to an unsolveable system of equations"
                         , pCircuitNetList->nodeNameAry[pDev->idxNodeOpOut]
                         , pDev->name
                         );
            }

            const subNetwork_t * const pSubNetOpOut = lookForSubNetByNode( pNetwork
                                                                         , pDev->idxNodeOpOut
                                                                         );
            if( pSubNetOpOut != lookForSubNetByNode(pNetwork, pDev->idxNodeFrom)
                ||  pSubNetOpOut != lookForSubNetByNode(pNetwork, pDev->idxNodeTo)
              )
            {
                success = false;
                LOG_ERROR( _log
                         , "The three connectors of op-amp %s are connected to nodes that"
                           " belong to different unconnected sub-networks. The voltage"
                           " potential difference between such nodes and hence the"
                           " behaviour of the circuit is undefined"
                         , pDev->name
                         )
            }
            break;
          }

        case pci_devType_srcUByU:
        case pci_devType_srcIByU:
          {
            const subNetwork_t * const pSubNetCtrlPlus =
                                                lookForSubNetByNode( pNetwork
                                                                   , pDev->idxNodeCtrlPlus
                                                                   );
            if(pSubNetCtrlPlus != lookForSubNetByNode(pNetwork, pDev->idxNodeCtrlMinus))
            {
                success = false;
                LOG_ERROR( _log
                         , "The control inputs of %s (%s) are connected to nodes that"
                           " belong to different unconnected sub-networks. The voltage"
                           " potential difference between such nodes and hence the"
                           " behaviour of the circuit is undefined"
                         , pDev->name
                         , pci_getNameOfDeviceType(pDev)
                         )
            }

            pNetwork->hasControlledSources = true;
            break;
          }

        case pci_devType_srcUByI:
        case pci_devType_srcIByI:
            pNetwork->hasControlledSources = true;
            break;

        default:
            break;

        } /* End switch(Do we have a kind of device, which requires a connectivity check?) */

    } /* End for(All devices to check for valid connections) */
    
    /* Continue the check, check the user-defined voltages. */
    unsigned int idxVolDef;
    for(idxVolDef=0; idxVolDef<pCircuitNetList->noVoltageDefs; ++idxVolDef)
    {
        const pci_voltageDef_t * const pVolDef = &pCircuitNetList->voltageDefAry[idxVolDef];
        const subNetwork_t * const pSubNetPlus = lookForSubNetByNode( pNetwork
                                                                    , pVolDef->idxNodePlus
                                                                    );
            if(pSubNetPlus != lookForSubNetByNode(pNetwork, pVolDef->idxNodeMinus))
            {
                success = false;
                LOG_ERROR( _log
                         , "The user-defined voltage %s is defined as voltage potential"
                           " difference between two nodes that"
                           " belong to different unconnected sub-networks. The voltage"
                           " potential difference between such nodes and hence the"
                           " meaning of the voltage is undefined"
                         , pVolDef->name
                         )
            }
    } /* End for(All user-defined voltages to check against cross sub-graph definition) */

    /* Write a representation of the found understanding of the network into the log. The
       messages are of verbosity level DEBUG, so it doesn't make sense to do this on level
       WARN or higher. */
    if(log_checkLogLevel(_log, log_debug))
        logNetworkTopology(pNetwork);

    if(success)
        *ppNetwork = pNetwork;
    else
    {
        LOG_ERROR(_log, "Please correct your circuit and re-run the application")
        deleteNetwork(pNetwork);
        *ppNetwork = NULL;
    }

    return success;

} /* End of analyseNetworkTopology */



/**
 * Free all data allocated in the data structure representing the network graph.
 *   @param pNetwork
 * The pointer to the data structure to de-allocate. No action if this is NULL.
 */

static void deleteNetwork(network_t * const pNetwork)
{
    if(pNetwork == NULL)
        return;

    /* Delete the reference to the net list representing the circuit. */
    assert(pNetwork->pCircuitNetList != NULL);
    pci_deleteParseResult(pNetwork->pCircuitNetList);

    subNetwork_t *pSubNet = pNetwork->pHeadOfSubNetList;
    while(pSubNet != NULL)
    {
        /* Go to the next sub-graph and free the current one. */
        subNetwork_t *pSubNetToFree = pSubNet;
        pSubNet = pSubNet->pNext;
        mem_free(_hMemMgrSubNet, pSubNetToFree);

    } /* End while(All sub-graphs) */

    /* Delete the array of node reference objects. */
    free(pNetwork->nodeRefAry);

    /* Delete the wrapping object, which had been allocated on the general purpose heap. */
    free(pNetwork);

} /* End of deleteNetwork */




/**
 * Select one node out of all nodes of a sub-grapgh, which shall have voltage null.
 * Basically any node can be defined to play this roll. However, typical use cases will
 * define a node to have a name like gnd or ground. If we can find such a node, we will use
 * it otherwise we (arbitrarily) use the first node.\n
 *   There's one hard condition to be fulfillied: The ouput of the op-amp must not be
 * connected to the ground node; this is because of the way the op-amp is expressed as a
 * linear equation.
 *   @return
 * \a true if a suitable ground node could be determined or \a false otherwise.
 *   @param pIdxGroundNode
 * The index of the ground node pointing into the array of nodes in the net list
 * representing the circuit is returned as * \a pIdxGroundNode.\n
 *   If no suitable ground node can be found then PCI_NULL_NODE is returned.
 *   @param pCircuitNetList
 * The net list as returned by the input file parser.
 *   @param pHeadOfNodeList
 * The list of nodes belonging to the sub-graph.
 */

static boolean findNodeGnd( unsigned int * const pIdxGroundNode
                          , const pci_circuit_t * const pCircuitNetList
                          , const networkNodeRef_t * const pHeadOfNodeList
                          )
{
    assert(pHeadOfNodeList != NULL);
    const networkNodeRef_t *pNode = pHeadOfNodeList;

    boolean success = true;

    /* If at least one op-amp is in the circuit then the user needs to unambiguously define
       the ground node; the op-amp is modelled as a controlled voltage source between
       ground and the op-amp's output and this evidently depends on the selection of the
       ground node. */
    boolean opAmpInCircuit = false;

    /* Respect the user's choice: We do a simple string compare on the nodes' names and
       look one called gnd or similar. */
    *pIdxGroundNode = PCI_NULL_NODE;
    do
    {
        /* Check name to see if the user wants to use this node as a ground node. */
        const char *nodeName = pCircuitNetList->nodeNameAry[pNode->idxNode];
        if(strstr(nodeName, "gnd") != NULL  ||  strstr(nodeName, "Gnd") != NULL
           || strstr(nodeName, "GND") != NULL
           || strstr(nodeName, "ground") != NULL  ||  strstr(nodeName, "Ground") != NULL
           || strstr(nodeName, "GROUND") != NULL
          )
        {
            if(*pIdxGroundNode == PCI_NULL_NODE)
            {
                /* The ground node must not be the output of an op-amp. Otherwise we loose
                   the only equation about its unknown output current. This condition is
                   superior. */
                if(!pNode->isOpampOutput)
                    *pIdxGroundNode = pNode->idxNode;
                else
                {
                    success = false;
                    LOG_ERROR( _log
                             , "The specified node %s can't be used as ground node as an"
                               " op-amp's output is connected to this node"
                             , nodeName
                             )
                }
            }
            else
            {
                success = false;
                LOG_ERROR( _log
                         , "The ground node is ambiguously defined. Please specify only one"
                           " ground node for each unconnected sub-network"
                         )
            }
        }

        if(pNode->isOpampOutput)
            opAmpInCircuit = true;

        pNode = pNode->pNext;
    }
    while(success &&  pNode != NULL);

    /* In case we won't find a better node by name, then the first node is taken as ground
       node. However, free choice of the ground node is not possible if an op-amp is in
       use. */
    if(success &&  *pIdxGroundNode == PCI_NULL_NODE)
    {
        if(!opAmpInCircuit)
            *pIdxGroundNode = pHeadOfNodeList->idxNode;
        else
            success = false;
    }

    if(!success)
    {
        LOG_ERROR( _log
                 , "No suitable ground node could be found in the circuit or in one of its"
                   " sub-networks. A ground node is a node whose name contains"
                   " either \"gnd\" or \"ground\" and which is not connected to the output"
                   " of an op-amp. The ground node needs to be unambiguously defined within"
                   " a sub-network"
                 )
    }
    else
        assert(*pIdxGroundNode < pCircuitNetList->noNodes);

    return success;

} /* End of findNodeGnd */



/**
 * The user input file doesn't define the name of the unknowns of the LES, but the meaning
 * of the unknowns is closely related to the user specified elements. Therefore we have a
 * function, that derives name sfor unknowns from the user specified elements.
 *   @param name
 * The character which to place the generate name in.
 *   @param maxLen
 * The maximum generated string length. name is as least one character larger.
 *   @param isVoltage
 * The unknowns are either voltages or currents and the name depends on the physical
 * quantity.
 *   @param userObject
 * The name of the user specified object the unknown is related to.
 */

static void createNameOfUnknown( char name[]
                               , unsigned int maxLen
                               , boolean isVoltage
                               , const char * const userObject
                               )
{
    assert(maxLen > sizeof(SYMBOL_PREFIX_VOLTAGE)  &&  maxLen > sizeof(SYMBOL_PREFIX_CURRENT));
    if(isVoltage)
    {
        strcpy(name, SYMBOL_PREFIX_VOLTAGE);
        maxLen -= sizeof(SYMBOL_PREFIX_VOLTAGE) - 1;
    }
    else
    {
        strcpy(name, SYMBOL_PREFIX_CURRENT);
        maxLen -= sizeof(SYMBOL_PREFIX_CURRENT) - 1;
    }

    /* Different to the (basically better suited strncpy) will strncat always write a
       terminating null character. */
    strncat(name, userObject, maxLen);

} /* End of createNameOfUnknown */




/**
 * Find out how many and which known and unknown variables and how many constants are
 * required in the LES describing the network. This is pass one of making the LES. After
 * return, we have a data structure describing all variables and constants in use.
 *   @return
 * The function will indicate failure by return value \a false, e.g. if the permitted
 * number of constants or knowns or unknowns is exceeded.
 *   @param ppTableOfVars
 * The reference to the table of variables is passed. The table is created at the beginning
 * and filled with all knowns, unknowns and constants found in the network.
 *   @param pNetwork
 * The representation of the network as got from the network topology analysis. Contains
 * the net list representing the circuit and the list of sub-graphs that form the complete
 * network.
 *   @remark
 * The table of variables is allocated dynamically. The data structure needs to be freed
 * after use. Use destructor void tbv_deleteTableOfVariables(tbv_tableOfVariables_t * const)
 * to do so.
 */

static boolean determineReqVariables( tbv_tableOfVariables_t * * const ppTableOfVars
                                    , const network_t * const pNetwork
                                    )
{
    unsigned int noKnowns = 0
               , noUnknowns = 0
               , noConstants = 0;
    const pci_circuit_t * const pCircuitNetList = pNetwork->pCircuitNetList;

    /* The number of nodes is the number of unknown voltages if we subtract one dependent
       voltage from each unconnected sub-graph. */
    assert(pCircuitNetList->noNodes >= pNetwork->noSubNets);
    noUnknowns = pCircuitNetList->noNodes - pNetwork->noSubNets;

    /* Most devices are characterized (or quantified) by a constant. Some special devices
       introduce an additional unknown and constant sources are the knowns of the LES.
       Pass one: Inspect all devices and count. */
    unsigned int idxDev;
    for(idxDev=0; idxDev<pCircuitNetList->noDevices; ++idxDev)
    {
        const pci_device_t * const pDev = pCircuitNetList->pDeviceAry[idxDev];
        switch(pDev->type)
        {
        case pci_devType_resistor    : ++ noConstants; break;
        case pci_devType_conductance : ++ noConstants; break;
        case pci_devType_capacitor   : ++ noConstants; break;
        case pci_devType_inductivity : ++ noConstants; break;
        case pci_devType_opAmp       : ++ noUnknowns; break;
        case pci_devType_srcU        : ++ noKnowns; ++ noUnknowns ; break;
        case pci_devType_srcUByU     : ++ noConstants; ++ noUnknowns ; break;
        case pci_devType_srcUByI     : ++ noConstants; ++ noUnknowns ; break;
        case pci_devType_srcI        : ++ noKnowns; break;
        case pci_devType_srcIByU     : ++ noConstants; break;
        case pci_devType_srcIByI     : ++ noConstants; break;
        case pci_devType_currentProbe: ++ noUnknowns; break;

        default: assert(false);
        }
    } /* End for(All devices) */

    /* Now we can create the empty table of variables. The net list representing the
       circuit, which the table is related to, is made accessible from the table object. */
    tbv_tableOfVariables_t * const pTabOfVars = tbv_createTableOfVariables( noKnowns
                                                                          , noUnknowns
                                                                          , noConstants
                                                                          , pCircuitNetList
                                                                          );

    /* Add the unknowns for the node voltages. To find only independent node voltages we
       need to iterate through the entire network; begin with its unconnected sub-graphs. */
    boolean success = true;
    const subNetwork_t *pSubNet = pNetwork->pHeadOfSubNetList;
    unsigned int idSubNet = 1;
    while(pSubNet != NULL)
    {
        unsigned int idxNodeGnd;
        if(!findNodeGnd(&idxNodeGnd, pCircuitNetList, pSubNet->pHeadOfNodeList))
            success = false;
        else
        {
            LOG_INFO( _log
                    , "Node %s is considered a ground node. The voltage at this"
                      " node is considered null"
                    , pCircuitNetList->nodeNameAry[idxNodeGnd]
                    )
        }

        const networkNodeRef_t *pNodeRef = pSubNet->pHeadOfNodeList;
        while(success &&  pNodeRef != NULL)
        {
            unsigned int idxNode = pNodeRef->idxNode;

            /* The voltage of each non ground node is one unknown. */
            if(idxNode == idxNodeGnd)
            {
                pNodeRef = pNodeRef->pNext;
                continue;
            }

            /* Find the name of the unknown, which describes the voltage at the node. */
            const unsigned int maxSizeOfName = MAX_SIZE_OF_SYMBOL_PREFIX
                                               + strlen(pCircuitNetList->nodeNameAry[idxNode]);
            char nameUnknown[maxSizeOfName];
            createNameOfUnknown( nameUnknown
                               , maxSizeOfName
                               , /* isVoltage */ true
                               , /* userObject */ pCircuitNetList->nodeNameAry[idxNode]
                               );

            /* Add the voltage to the list of unknowns. We may continue in case of an error
               here in order to collect all errors in the input. */
            if(!tbv_addUnknown( pTabOfVars
                              , nameUnknown
                              , idxNode
                              , idSubNet
                              , PCI_NULL_DEVICE
                              )
              )
            {
                success = false;
            }

            pNodeRef = pNodeRef->pNext;
        }

        pSubNet = pSubNet->pNext;
        ++ idSubNet;

    } /* End while(All unconnected sub-graphs) */


    /* Second pass: Iterate through all devices. This time we take the information about
       the devices and place it in the meanwhile created data structure of fitting size. */
    for(idxDev=0; success && idxDev<pCircuitNetList->noDevices; ++idxDev)
    {
        const pci_device_t * const pDev = pCircuitNetList->pDeviceAry[idxDev];

        switch(pDev->type)
        {
        /* The passive devices just constitute a constant. */
        case pci_devType_resistor   :
        case pci_devType_conductance:
        case pci_devType_capacitor  :
        case pci_devType_inductivity:

        /* The controlled sources introduce their amplification factor as a constant. */
        case pci_devType_srcUByU:
        case pci_devType_srcUByI:
        case pci_devType_srcIByU:
        case pci_devType_srcIByI:
            tbv_addConstant(pTabOfVars, idxDev);
            break;

        default:
            break;
        }

        /* The op-amp is ideal and doesn't constitute a (physical) constant but it is
           handled by an additional equation and unknown.
             The voltage sources are handled by an additional equation and unknown. */
        switch(pDev->type)
        {
        case pci_devType_opAmp       :
        case pci_devType_srcU        :
        case pci_devType_srcUByU     :
        case pci_devType_srcUByI     :
        case pci_devType_currentProbe:
            {
                /* Usually, we append a I_ to the device name in order to derive the name
                   of an unknown current flowing though this device. This is intuitive and
                   resonable for the op-amp and the voltage sources but counter-intuitive
                   for the current probes:
                     Typically the voltage source will be named U1, Uin, Uload etc. which
                   leads to I_U1, I_Uin, I_Uload as current designations. The output
                   current of an op-amp would e.g. become I_OP1, which is also okay.
                     For current probes one would easily give it a name like Ibase, Iin,
                   Iout if these are the sensed currents. Out naming scheme would lead to
                   currents like I_Ibase, I_Iin, I_Iout in the result formulas. The user
                   has to chose the probe's name by anticipating the naming scheme and give
                   it unsatisfying names like base, in or out or - as a kind of compromise
                   between meaningful result formulas and meaningful input files - PI_base,
                   PI_in, PI_out. Now we would get the bulky but somehow reasonable
                   current designations I_PI_base, I_PI_in, I_PI_out.
                     To avoid this, we allow a deviating naming scheme for current probes.
                   If their name already starts with I_ then we do not prefix it but take
                   the device name directly as current name. */
                const char *devName = pDev->name;
                if(pDev->type == pci_devType_currentProbe
                   &&  strncmp( pDev->name
                              , SYMBOL_PREFIX_CURRENT
                              , sizeof(SYMBOL_PREFIX_CURRENT)-1
                              ) == 0
                   &&  strlen(pDev->name)+1 > sizeof(SYMBOL_PREFIX_CURRENT)
                  )
                {
                    devName += sizeof(SYMBOL_PREFIX_CURRENT)-1;
                }
                
                const unsigned int maxSizeOfName = MAX_SIZE_OF_SYMBOL_PREFIX
                                                   + strlen(pDev->name);
                char nameUnknown[maxSizeOfName];
                createNameOfUnknown( nameUnknown
                                   , maxSizeOfName
                                   , /* isVoltage */ false
                                   , /* userObject */ devName
                                   );

                /* Add the current to the list of unknowns. The reference to a node by
                   index is meaningless in this case. */
                if(!tbv_addUnknown( pTabOfVars
                                  , nameUnknown
                                  , /* idxNode */ PCI_NULL_NODE
                                  , /* idSubNet */ UINT_MAX
                                  , /* idxDevice */ idxDev
                                  )
                  )
                {
                    success = false;
                }
            }

            break;

        default:
            break;
        }

        /* The constant sources are the knowns of the LES. */
        switch(pDev->type)
        {
        case pci_devType_srcU:
        case pci_devType_srcI:
            /* Add the given input variable to the list of knowns. */
            if(!tbv_addKnown(pTabOfVars, pDev->name, idxDev))
                success = false;

            break;

        default:
            break;
        }

    } /* End for(All devices) */

    /* Now we have the list of all device constants in use. We can sort the internal order
       so that later result output will present them in the common order: R, L, C. */
    if(success)
    {
        tbv_sortConstants(pTabOfVars);
        assert(pTabOfVars->noKnowns == noKnowns  &&  pTabOfVars->noUnknowns == noUnknowns
               &&  pTabOfVars->noConstants == noConstants
              );
    }

    LOG_INFO( _log
            , "Linear equation system (%u, %u) has %u knowns, %u unknowns and %u constants"
            , noUnknowns
            , noUnknowns + noKnowns
            , noKnowns
            , noUnknowns
            , noConstants
            );

    if(noConstants > COE_MAX_NO_CONST)
    {
        success = false;
        LOG_ERROR( _log
                 , "Maximum supported number %u of constants (or devices)"
                   " is exceeded. No computation can be carried out. Please consider to"
                   " simplify your circuit"
                 , COE_MAX_NO_CONST
                 );
    }

    *ppTableOfVars = pTabOfVars;
    return success;

} /* End of determineReqVariables */





/**
 * Add those terms to the (half way completed) LES, that describe the conditions
 * superimposed by a constant voltage source.
 *   @param A
 * The matrix m*n of pointers to coefficients.
 *   @param pTableOfVars
 * The table of all constants, knowns and unknows, which are used in the LES by reference.
 *   @param idxDevice
 * The index of the device under progress. The equation and/or terms of equations for
 * this device are added to the LES.
 */

static void addSrcUConditions( coe_coefMatrix_t const A
                             , const tbv_tableOfVariables_t * const pTableOfVars
                             , unsigned int idxDevice
                             )
{
    assert(idxDevice < pTableOfVars->pCircuitNetList->noDevices);
    const pci_device_t * const pDevice = pTableOfVars->pCircuitNetList->pDeviceAry[idxDevice];
    assert(pDevice->type == pci_devType_srcU);
    assert(pDevice->idxNodeFrom < pTableOfVars->pCircuitNetList->noNodes
           &&  pDevice->idxNodeTo < pTableOfVars->pCircuitNetList->noNodes
           &&  pDevice->idxNodeCtrlPlus == PCI_NULL_NODE
           &&  pDevice->idxNodeCtrlMinus == PCI_NULL_NODE
          );

    /* For voltage sources we need to define the polarity: The "from" node is its positive,
       the "to" node its negative end. */

    /* The voltage source is handled by an additional internal unknown, the current
       flowing out of the positive end of the source. */
    const tbv_unknownVariable_t * const pUnknownI = tbv_getUnknownByDevice( pTableOfVars
                                                                          , idxDevice
                                                                          );
    assert(pUnknownI != NULL);
    const unsigned int idxColUnknownI = pUnknownI->idxCol;

    /* Extend the current balances of the two connected nodes by a new term
       describing the current flowing through the voltage source.
         Don't do this, if the connected node is a ground node, i.e. if the returned
       unknown is the NULL pointer. */
    const tbv_unknownVariable_t * const pUnknownUFrom = tbv_getUnknownByNode
                                                                ( pTableOfVars
                                                                , pDevice->idxNodeFrom
                                                                );
    if(pUnknownUFrom != NULL)
    {
        /* factor: The current is flowing into the node the device's pin "from" is
           connected to; this is by definition a positive current. */
        coe_addAddend( &A[pUnknownUFrom->idxRow][idxColUnknownI]
                     , /* factor */          +1 
                     , /* productOfConsts */ 0
                     );
    }
    /* else: Connected node is a ground node. */

    const tbv_unknownVariable_t * const pUnknownUTo = tbv_getUnknownByNode( pTableOfVars
                                                                          , pDevice->idxNodeTo
                                                                          );
    if(pUnknownUTo != NULL)
    {
        /* factor: The current is flowing from the node the device's pin "to" is connected to
           back into the voltage source; this is by definition a negative current. */
        coe_addAddend( &A[pUnknownUTo->idxRow][idxColUnknownI]
                     , /* factor */          -1
                     , /* productOfConsts */ 0
                     );
    }
    /* else: Connected node is a ground node. */

    /* The additional unknown requires an additional equation. This equation says,
       that the voltage difference between the two connectors is the given, known voltage
       of the source. The equation to make is:
         U(nodeFrom) - U(nodeTo) - U0 = 0,
       where U0 is the known voltage of the source, a system input. Both U(nodeX) can be
       null, if this node is the ground node. In which case the term simply doesn't appear
       in the made equation. */
    const unsigned int idxEqSuppl = pUnknownI->idxRow;
    if(pUnknownUFrom != NULL)
    {
        coe_addAddend( &A[idxEqSuppl][pUnknownUFrom->idxCol]
                     , /* factor */          +1
                     , /* productOfConsts */ 0
                     );
    }
    /* else: Node connected to "from" is a ground node. */

    if(pUnknownUTo != NULL)
    {
        coe_addAddend( &A[idxEqSuppl][pUnknownUTo->idxCol]
                     , /* factor */          -1
                     , /* productOfConsts */ 0
                     );
    }
    /* else: Node connected to "to" is a ground node. */

    const unsigned int idxColKnownU = tbv_getKnownByDevice(pTableOfVars, idxDevice)->idxCol;
    coe_addAddend( &A[idxEqSuppl][idxColKnownU]
                 , /* factor */          -1
                 , /* productOfConsts */ 0
                 );
} /* End of addSrcUConditions */




/**
 * Add those terms to the (half way completed) LES, that describe the conditions
 * superimposed by a constant current source.
 *   @param A
 * The matrix m*n of pointers to coefficients.
 *   @param pTableOfVars
 * The table of all constants, knowns and unknows, which are used in the LES by reference.
 *   @param idxDevice
 * The index of the device under progress. The equation and/or terms of equations for
 * this device are added to the LES.
 */

static void addSrcIConditions( coe_coefMatrix_t const A
                             , const tbv_tableOfVariables_t * const pTableOfVars
                             , unsigned int idxDevice
                             )
{
    assert(idxDevice < pTableOfVars->pCircuitNetList->noDevices);
    const pci_device_t * const pDevice = pTableOfVars->pCircuitNetList->pDeviceAry[idxDevice];
    assert(pDevice->type == pci_devType_srcI);
    assert(pDevice->idxNodeFrom < pTableOfVars->pCircuitNetList->noNodes
           &&  pDevice->idxNodeTo < pTableOfVars->pCircuitNetList->noNodes
           &&  pDevice->idxNodeCtrlPlus == PCI_NULL_NODE
           &&  pDevice->idxNodeCtrlMinus == PCI_NULL_NODE
          );

    /* For current sources we need to define the polarity: The (positive defined) current
       is flowing into the pin connected to node "from" node and it is flowing out of the
       source into node "to". This definition sounds inverse but is intuitive as the arrow
       in the symbol of the source has the orientation from "from" to "to". */

    /* Get the column of the LES, which holds the known current. */
    unsigned int idxColKnownI = tbv_getKnownByDevice(pTableOfVars, idxDevice)->idxCol;

    /* Extend the current balances of the two connected nodes by a new term
       describing the current flowing through the current source.
         Don't do this, if the connected node is a ground node, i.e. if the returned
       unknown is the NULL pointer. */
    const tbv_unknownVariable_t * const pUnknownUFrom = tbv_getUnknownByNode
                                                                ( pTableOfVars
                                                                , pDevice->idxNodeFrom
                                                                );
    if(pUnknownUFrom != NULL)
    {
        /* factor: The current is flowing into the node the device's pin "from" is
           connected to; this is by definition a positive current. */
        coe_addAddend( &A[pUnknownUFrom->idxRow][idxColKnownI]
                     , /* factor */          -1
                     , /* productOfConsts */ 0
                     );
    }
    /* else: Connected node is a ground node. */

    const tbv_unknownVariable_t * const pUnknownUTo = tbv_getUnknownByNode( pTableOfVars
                                                                          , pDevice->idxNodeTo
                                                                          );
    if(pUnknownUTo != NULL)
    {
        /* factor: The current is flowing from the node the device's pin "to" is connected
           to back into the current source; this is by definition a negative current. */
        coe_addAddend( &A[pUnknownUTo->idxRow][idxColKnownI]
                     , /* factor */          +1
                     , /* productOfConsts */ 0
                     );
    }
    /* else: Connected node is a ground node. */

} /* End of addSrcIConditions */




/**
 * Add those terms to the (half way completed) LES, that describe the conditions
 * superimposed by a passive device (R, Y, L, C).
 *   @param A
 * The matrix m*n of pointers to coefficients.
 *   @param pTableOfVars
 * The table of all constants, knowns and unknows, which are used in the LES by reference.
 *   @param idxDevice
 * The index of the device under progress. The terms of the equations for this device are
 * added to the LES.
 */

static void addPassiveDeviceConditions( coe_coefMatrix_t const A
                                      , const tbv_tableOfVariables_t * const pTableOfVars
                                      , unsigned int idxDevice
                                      )
{
    assert(idxDevice < pTableOfVars->pCircuitNetList->noDevices);
    const pci_device_t * const pDevice = pTableOfVars->pCircuitNetList->pDeviceAry[idxDevice];

    assert(pDevice->idxNodeFrom < pTableOfVars->pCircuitNetList->noNodes
           &&  pDevice->idxNodeTo < pTableOfVars->pCircuitNetList->noNodes
           &&  pDevice->idxNodeCtrlPlus == PCI_NULL_NODE
           &&  pDevice->idxNodeCtrlMinus == PCI_NULL_NODE
          );

    const coe_productOfConst_t k = tbv_getConstantByDevice(pTableOfVars, idxDevice);
    const tbv_unknownVariable_t * const pUnknownUFrom = tbv_getUnknownByNode
                                                            ( pTableOfVars
                                                            , pDevice->idxNodeFrom
                                                            )
                              , * const pUnknownUTo = tbv_getUnknownByNode
                                                            ( pTableOfVars
                                                            , pDevice->idxNodeTo
                                                            );

    /* Extend the current balances of the two connected nodes. Each balance gets two new
       terms. Each term describes the current flowing into the node, which the balance is
       made for, because of the voltage potential of the connected node. If the latter is
       identical to the node, which the balance is made for, then the current is effluent,
       otherwise influent. Influent currents are defined to be positive.
         The terms disappear, if the connected node is a ground node. Therefore we will
       either see four terms or only one term. */

    /* Extend the current balance at node "from" - if this is not a ground node. */
    if(pUnknownUFrom != NULL)
    {
        /* Add a new added term to the sum, which is the coefficient of the LES, that
           belongs to the voltage of node "from". The sign of the current is negative, an
           increased voltage leads to effluent current. This flow direction is defined to
           be negative. */
        coe_addAddend( &A[pUnknownUFrom->idxRow][pUnknownUFrom->idxCol]
                     , /* factor */          -1
                     , /* productOfConsts */ k
                     );

        if(pUnknownUTo != NULL)
        {
            /* Add a new added term to the sum, which is the coefficient of the LES, that
               belongs to the voltage of node "to". The term describes the impact of the
               far end of the device. An increased voltage potential of the other node "to"
               leads to an influent current at "from". This flow direction is defined to be
               positive. */
            coe_addAddend( &A[pUnknownUFrom->idxRow][pUnknownUTo->idxCol]
                         , /* factor */          +1
                         , /* productOfConsts */ k
                         );
        }
    }
    /* else: Connected node "from" is a ground node. */

    /* Extend the current balance at node "to" - if this is not a ground node. */
    if(pUnknownUTo != NULL)
    {
        /* Add a new added term to the sum, which is the coefficient of the LES, that
           belongs to the voltage of node "to". The sign of the current is negative, an
           increased voltage leads to effluent current. This flow direction is defined to
           be negative. */
        coe_addAddend( &A[pUnknownUTo->idxRow][pUnknownUTo->idxCol]
                     , /* factor */          -1
                     , /* productOfConsts */ k
                     );

        if(pUnknownUFrom != NULL)
        {
            /* Add a new added term to the sum, which is the coefficient of the LES, that
               belongs to the voltage of node "from". The term describes the impact of the
               far end of the device. An increased voltage potential of the other node "from"
               leads to an influent current at "to". This flow direction is defined to be
               positive. */
            coe_addAddend( &A[pUnknownUTo->idxRow][pUnknownUFrom->idxCol]
                         , /* factor */          +1
                         , /* productOfConsts */ k
                         );
        }
    }
    /* else: Connected node "to" is a ground node. */

} /* End of addPassiveDeviceConditions */




/**
 * Add those terms to the (half way completed) LES, that describe the conditions
 * superimposed by an operational amplifier.
 *   @param A
 * The matrix m*n of pointers to coefficients.
 *   @param pTableOfVars
 * The table of all constants, knowns and unknows, which are used in the LES by reference.
 *   @param idxDevice
 * The index of the device under progress. The equation and/or terms of equations for
 * this device are added to the LES.
 */

static void addOpAmpConditions( coe_coefMatrix_t const A
                              , const tbv_tableOfVariables_t * const pTableOfVars
                              , unsigned int idxDevice
                              )
{
    assert(idxDevice < pTableOfVars->pCircuitNetList->noDevices);
    const pci_device_t * const pDevice = pTableOfVars->pCircuitNetList->pDeviceAry[idxDevice];
    assert(pDevice->type == pci_devType_opAmp);

    /* The op-amp is described like this: It doesn't affect the current balance of its
       inputs at all; the inputs have infinite impedance. If the algorithm visits the input
       nodes, we can thus return immediately.
         The output injects an unknown current into the connected node. This current is an
       additional unknown, introduced by the op-amp. Accordingly, we need an additional
       equation. This equation says that the voltage of both input nodes is equal. */
    const tbv_unknownVariable_t * const pUnknownI = tbv_getUnknownByDevice( pTableOfVars
                                                                          , idxDevice
                                                                          );
    assert(pUnknownI != NULL);
    const unsigned int idxColUnknownI = pUnknownI->idxCol;


    /* Extend the current balance of the output node: Add the term of an influent current
       to the output node.
         Do this unconditionally; The connected node must not be a ground node - otherwise
       we would omit the only equation describing this current. (This has been considered
       when choosing the ground nodes.) */
    const tbv_unknownVariable_t * const pUnknownOpOut = tbv_getUnknownByNode
                                                                ( pTableOfVars
                                                                , pDevice->idxNodeOpOut
                                                                );
    assert(pUnknownOpOut != NULL);
    /* factor: The current is defined to flow into the node, this is positive by
       definition. */
    coe_addAddend( &A[pUnknownOpOut->idxRow][idxColUnknownI]
                 , /* factor */          +1
                 , /* productOfConsts */ 0
                 );

    /* The additional unknown requires an additional equation. This equation says,
       that the voltage difference between the two inputs is null. The equation to make is:
         U(nodeFrom) - U(nodeTo) = 0
       Both U(nodeX) can be null, if this node is the ground node. In which case the term
       simply doesn't appear in the made equation. */
    const unsigned int idxEqSuppl = pUnknownI->idxRow;
    const tbv_unknownVariable_t * const pUnknownUFrom = tbv_getUnknownByNode
                                                                ( pTableOfVars
                                                                , pDevice->idxNodeFrom
                                                                );
    const tbv_unknownVariable_t * const pUnknownUTo = tbv_getUnknownByNode( pTableOfVars
                                                                          , pDevice->idxNodeTo
                                                                          );
    if(pUnknownUFrom != NULL)
    {
        coe_addAddend( &A[idxEqSuppl][pUnknownUFrom->idxCol]
                     , /* factor */          +1
                     , /* productOfConsts */ 0
                     );
    }
    /* else: Node connected to "from" is a ground node. */

    if(pUnknownUTo != NULL)
    {
        coe_addAddend( &A[idxEqSuppl][pUnknownUTo->idxCol]
                     , /* factor */          -1
                     , /* productOfConsts */ 0
                     );
    }
    /* else: Node connected to "to" is a ground node. */

} /* End of addOpAmpConditions */




/**
 * Add those terms to the (half way completed) LES, that describe the conditions
 * superimposed by a current probe.
 *   @param A
 * The matrix m*n of pointers to coefficients.
 *   @param pTableOfVars
 * The table of all constants, knowns and unknows, which are used in the LES by reference.
 *   @param idxDevice
 * The index of the device under progress. The equation and/or terms of equations for
 * this device are added to the LES.
 */

static void addCurrentProbeConditions( coe_coefMatrix_t const A
                                     , const tbv_tableOfVariables_t * const pTableOfVars
                                     , unsigned int idxDevice
                                     )
{
    assert(idxDevice < pTableOfVars->pCircuitNetList->noDevices);
    const pci_device_t * const pDevice = pTableOfVars->pCircuitNetList->pDeviceAry[idxDevice];
    assert(pDevice->type == pci_devType_currentProbe);
    assert(pDevice->idxNodeFrom < pTableOfVars->pCircuitNetList->noNodes
           &&  pDevice->idxNodeTo < pTableOfVars->pCircuitNetList->noNodes
           &&  pDevice->idxNodeCtrlPlus == PCI_NULL_NODE
           &&  pDevice->idxNodeCtrlMinus == PCI_NULL_NODE
          );

    /* The current probe is handled by an additional internal unknown, the current
       flowing into the node "from" of the probe. */
    const tbv_unknownVariable_t * const pUnknownI = tbv_getUnknownByDevice( pTableOfVars
                                                                          , idxDevice
                                                                          );
    assert(pUnknownI != NULL);
    const unsigned int idxColUnknownI = pUnknownI->idxCol;

    /* Extend the current balances of the two connected nodes by a new term
       describing the current flowing through the probe.
         Don't do this, if the connected node is a ground node, i.e. if the returned
       unknown is the NULL pointer. */
    const tbv_unknownVariable_t * const pUnknownUFrom = tbv_getUnknownByNode
                                                                ( pTableOfVars
                                                                , pDevice->idxNodeFrom
                                                                );
    if(pUnknownUFrom != NULL)
    {
        /* factor: The unknown probe current is defined to flow into the probe's node
           "from"; it's hence affluent with respect to the node and this is by definition a
           negative contribution to the node's current balance. */
        coe_addAddend( &A[pUnknownUFrom->idxRow][idxColUnknownI]
                     , /* factor */          -1
                     , /* productOfConsts */ 0
                     );
    }
    /* else: Connected node is a ground node. */

    const tbv_unknownVariable_t * const pUnknownUTo = tbv_getUnknownByNode( pTableOfVars
                                                                          , pDevice->idxNodeTo
                                                                          );
    if(pUnknownUTo != NULL)
    {
        /* factor: The unknown probe current is defined to flow out of the probe's node
           "to"; it's hence inffluent with respect to this node and this is by definition a
           positive contribution to the node's current balance. */
        coe_addAddend( &A[pUnknownUTo->idxRow][idxColUnknownI]
                     , /* factor */          +1
                     , /* productOfConsts */ 0
                     );
    }
    /* else: Connected node is a ground node. */

    /* The additional unknown requires an additional equation. This equation says,
       that the current probe equalizes the voltage potentials between the two connectors.
       The equation to make is:
         U(nodeFrom) - U(nodeTo) = 0
       Both U(nodeX) can be null, if this node is the ground node. In which case the term
       simply doesn't appear in the made equation. */
    const unsigned int idxEqSuppl = pUnknownI->idxRow;
    if(pUnknownUFrom != NULL)
    {
        coe_addAddend( &A[idxEqSuppl][pUnknownUFrom->idxCol]
                     , /* factor */          +1
                     , /* productOfConsts */ 0
                     );
    }
    /* else: Node connected to "from" is a ground node. */

    if(pUnknownUTo != NULL)
    {
        coe_addAddend( &A[idxEqSuppl][pUnknownUTo->idxCol]
                     , /* factor */          -1
                     , /* productOfConsts */ 0
                     );
    }
    /* else: Node connected to "to" is a ground node. */

} /* End of addCurrentProbeConditions */




/**
 * Add those terms to the (half way completed) LES, that describe the conditions
 * superimposed by a voltage controlled voltage source.
 *   @param A
 * The matrix m*n of pointers to coefficients.
 *   @param pTableOfVars
 * The table of all constants, knowns and unknows, which are used in the LES by reference.
 *   @param idxDevice
 * The index of the device under progress. The equation and/or terms of equations for
 * this device are added to the LES.
 */

static void addSrcUByUConditions( coe_coefMatrix_t const A
                                , const tbv_tableOfVariables_t * const pTableOfVars
                                , unsigned int idxDevice
                                )
{
    assert(idxDevice < pTableOfVars->pCircuitNetList->noDevices);
    const pci_device_t * const pDevice = pTableOfVars->pCircuitNetList->pDeviceAry[idxDevice];
#ifdef DEBUG
    const unsigned int noNodes = pTableOfVars->pCircuitNetList->noNodes;
    assert(pDevice->idxNodeFrom < noNodes  &&  pDevice->idxNodeTo < noNodes
           &&  pDevice->idxNodeCtrlPlus < noNodes
           &&  pDevice->idxNodeCtrlMinus < noNodes
          );
#endif

    /* For voltage sources we need to define the polarity: The "from" node is its
       positive, the "to" node its negative end. */

    /* The voltage source is handled by an additional internal unknown, the current
       flowing out of the positive end of the source. */
    const tbv_unknownVariable_t * const pUnknownI = tbv_getUnknownByDevice( pTableOfVars
                                                                          , idxDevice
                                                                          );
    assert(pUnknownI != NULL);
    const unsigned int idxColUnknownI = pUnknownI->idxCol;

    /* Extend the current balances of the two connected nodes by a new term
       describing the current flowing through the voltage source.
         Don't do this, if the connected node is a ground node, i.e. if the returned
       unknown is the NULL pointer. */
    const tbv_unknownVariable_t * const pUnknownUFrom = tbv_getUnknownByNode
                                                                ( pTableOfVars
                                                                , pDevice->idxNodeFrom
                                                                );
    if(pUnknownUFrom != NULL)
    {
        /* factor: The current is flowing into the node the device's pin "from" is
           connected to; this is by definition a positive current. */
        coe_addAddend( &A[pUnknownUFrom->idxRow][idxColUnknownI]
                     , /* factor */          +1
                     , /* productOfConsts */ 0
                     );
    }
    /* else: Connected node is a ground node. */

    const tbv_unknownVariable_t * const pUnknownUTo = tbv_getUnknownByNode( pTableOfVars
                                                                          , pDevice->idxNodeTo
                                                                          );
    if(pUnknownUTo != NULL)
    {
        /* factor: The current is flowing from the node the device's pin "to" is connected
           to back into the voltage source; this is by definition a negative current. */
        coe_addAddend( &A[pUnknownUTo->idxRow][idxColUnknownI]
                     , /* factor */          -1
                     , /* productOfConsts */ 0
                     );
    }
    /* else: Connected node is a ground node. */

    /* The additional unknown requires an additional equation. This equation says,
       that the voltage difference between the two connectors is a certain voltage.
       The equation to make is:
         U(nodeFrom) - U(nodeTo) - k*U(nodeCtrlPlus) + k*U(nodeCtrlMinus) = 0,
       where k is the device constant of the source. Each out of the four U(nodeX)
       can be null, if this node is the ground node. In which case the term simply
       doesn't appear in the made equation. */
    coe_productOfConst_t k = tbv_getConstantByDevice(pTableOfVars, idxDevice);
    const unsigned int idxEqSuppl = pUnknownI->idxRow;

    if(pUnknownUFrom != NULL)
    {
        const unsigned int idxColNodeFrom = pUnknownUFrom->idxCol;
        coe_addAddend( &A[idxEqSuppl][idxColNodeFrom]
                     , /* factor */          +1
                     , /* productOfConsts */ 0
                     );
    }
    /* else: Node connected to "from" is a ground node. */

    if(pUnknownUTo != NULL)
    {
        const unsigned int idxColNodeTo = pUnknownUTo->idxCol;
        coe_addAddend( &A[idxEqSuppl][idxColNodeTo]
                     , /* factor */          -1
                     , /* productOfConsts */ 0
                     );
    }
    /* else: Node connected to "to" is a ground node. */

    const tbv_unknownVariable_t * const pUnknownUCtrlPlus = tbv_getUnknownByNode
                                                                ( pTableOfVars
                                                                , pDevice->idxNodeCtrlPlus
                                                                );
    if(pUnknownUCtrlPlus != NULL)
    {
        const unsigned int idxColNodeCtrlPlus = pUnknownUCtrlPlus->idxCol;
        coe_addAddend( &A[idxEqSuppl][idxColNodeCtrlPlus]
                     , /* factor */          -1
                     , /* productOfConsts */ k
                     );
    }
    /* else: Node connected to "ctrlPlus" is a ground node. */

    const tbv_unknownVariable_t * const pUnknownUCtrlMinus = tbv_getUnknownByNode
                                                                ( pTableOfVars
                                                                , pDevice->idxNodeCtrlMinus
                                                                );
    if(pUnknownUCtrlMinus != NULL)
    {
        const unsigned int idxColNodeCtrlMinus = pUnknownUCtrlMinus->idxCol;
        coe_addAddend( &A[idxEqSuppl][idxColNodeCtrlMinus]
                     , /* factor */          +1
                     , /* productOfConsts */ k
                     );
    }
    /* else: Node connected to "ctrlMinus" is a ground node. */

} /* End of addSrcUByUConditions */




/**
 * Add those terms to the (half way completed) LES, that describe the conditions
 * superimposed by a current controlled voltage source.
 *   @param A
 * The matrix m*n of pointers to coefficients.
 *   @param pTableOfVars
 * The table of all constants, knowns and unknows, which are used in the LES by reference.
 *   @param idxDevice
 * The index of the device under progress. The equation and/or terms of equations for
 * this device are added to the LES.
 */

static void addSrcUByIConditions( coe_coefMatrix_t const A
                                , const tbv_tableOfVariables_t * const pTableOfVars
                                , unsigned int idxDevice
                                )
{
    assert(idxDevice < pTableOfVars->pCircuitNetList->noDevices);
    const pci_device_t * const pDevice = pTableOfVars->pCircuitNetList->pDeviceAry[idxDevice];
    assert(pDevice->type == pci_devType_srcUByI);

    /* For voltage sources we need to define the polarity: The "from" node is its
       positive, the "to" node its negative end. */

    /* The voltage source is handled by an additional internal unknown, the current
       flowing out of the positive end of the source. */
    const tbv_unknownVariable_t * const pUnknownI = tbv_getUnknownByDevice( pTableOfVars
                                                                          , idxDevice
                                                                          );
    assert(pUnknownI != NULL);
    const unsigned int idxColUnknownI = pUnknownI->idxCol
                     , idxEqSuppl = pUnknownI->idxRow;

    /* Extend the current balances of the two connected nodes by a new term
       describing the current flowing through the voltage source.
         Don't do this, if the connected node is a ground node, i.e. if the returned
       unknown is the NULL pointer. */
    const tbv_unknownVariable_t * const pUnknownUFrom = tbv_getUnknownByNode
                                                                ( pTableOfVars
                                                                , pDevice->idxNodeFrom
                                                                );
    if(pUnknownUFrom != NULL)
    {
        /* factor: The current is flowing into the node the device's pin "from" is
           connected to; this is by definition a positive current. */
        coe_addAddend( &A[pUnknownUFrom->idxRow][idxColUnknownI]
                     , /* factor */          +1
                     , /* productOfConsts */ 0
                     );
    }
    /* else: Connected node is a ground node. */

    const tbv_unknownVariable_t * const pUnknownUTo = tbv_getUnknownByNode( pTableOfVars
                                                                          , pDevice->idxNodeTo
                                                                          );
    if(pUnknownUTo != NULL)
    {
        /* factor: The current is flowing from the node the device's pin "to" is connected
           to back into the voltage source; this is by definition a negative current. */
        coe_addAddend( &A[pUnknownUTo->idxRow][idxColUnknownI]
                     , /* factor */          -1
                     , /* productOfConsts */ 0
                     );
    }
    /* else: Connected node is a ground node. */

    /* The additional unknown requires an additional equation. This equation says,
       that the voltage difference between the two connectors is a certain voltage.
       The equation to make is:
         U(nodeFrom) - U(nodeTo) - k*Ictrl = 0,
       where k is the device constant of the source and Ictrl is the current through the
       related current probe. Each of the two U(nodeX) can be null, if this node is the
       ground node. In which case the term simply doesn't appear in the made equation. */
    if(pUnknownUFrom != NULL)
    {
        const unsigned int idxColNodeFrom = pUnknownUFrom->idxCol;
        coe_addAddend( &A[idxEqSuppl][idxColNodeFrom]
                     , /* factor */          +1
                     , /* productOfConsts */ 0
                     );
    }
    /* else: Node connected to "from" is a ground node. */

    if(pUnknownUTo != NULL)
    {
        const unsigned int idxColNodeTo = pUnknownUTo->idxCol;
        coe_addAddend( &A[idxEqSuppl][idxColNodeTo]
                     , /* factor */          -1
                     , /* productOfConsts */ 0
                     );
    }
    /* else: Node connected to "to" is a ground node. */

    /* The current controlled source refers to the current sensed by a current probe. We
       need to access the unknown current through this probe. */
    const tbv_unknownVariable_t * const pUnknownICtrl = tbv_getUnknownByDevice
                                                                ( pTableOfVars
                                                                , pDevice->idxCurrentProbe
                                                                );

    /* The current through the probe is multiplied with the device constant of the current
       controlled source. Access the constant. */
    const coe_productOfConst_t k = tbv_getConstantByDevice(pTableOfVars, idxDevice);
    coe_addAddend( &A[idxEqSuppl][pUnknownICtrl->idxCol]
                 , /* factor */          -1
                 , /* productOfConsts */ k
                 );

} /* End of addSrcUByIConditions */




/**
 * Add those terms to the (half way completed) LES, that describe the conditions
 * superimposed by a voltage controlled current source.
 *   @param A
 * The matrix m*n of pointers to coefficients.
 *   @param pTableOfVars
 * The table of all constants, knowns and unknows, which are used in the LES by reference.
 *   @param idxDevice
 * The index of the device under progress. The equation and/or terms of equations for
 * this device are added to the LES.
 */

static void addSrcIByUConditions( coe_coefMatrix_t const A
                                , const tbv_tableOfVariables_t * const pTableOfVars
                                , unsigned int idxDevice
                                )
{
    assert(idxDevice < pTableOfVars->pCircuitNetList->noDevices);
    const pci_device_t * const pDevice = pTableOfVars->pCircuitNetList->pDeviceAry[idxDevice];
    assert(pDevice->type == pci_devType_srcIByU);
#ifdef DEBUG
    const unsigned int noNodes = pTableOfVars->pCircuitNetList->noNodes;
    assert(pDevice->idxNodeFrom < noNodes  &&  pDevice->idxNodeTo < noNodes
           &&  pDevice->idxNodeCtrlPlus < noNodes
           &&  pDevice->idxNodeCtrlMinus < noNodes
          );
#endif

    /* For current sources we need to define the polarity: The (positive defined) current
       is flowing into the pin connected to node "from" node and it is flowing out of the
       source into node "to". This definition sounds inverse but is intuitive as the arrow
       in the symbol of the source has the orientation from "from" to "to". */

    /* The sensed control voltage is multiplied with the device constant of the voltage
       controlled source. Access the constant. */
    const coe_productOfConst_t k = tbv_getConstantByDevice(pTableOfVars, idxDevice);

    /* Extend the current balances of the two connected nodes by a new term
       describing the current flowing through the current source. The current is
         k*(U(nodeCtrlPlus) - U(nodeCtrlMinus)) = k*U(nodeCtrlPlus) - k*U(nodeCtrlMinus)
       for node "to" and with inverted sign for node "from". These are four terms to add in
       total. However, if the node "from" or "to" is a ground node then two of the terms
       are not needed (as the current balance of a ground node is linear dependent) and if
       one out of nodeCtrlX is a ground node than U(nodeCtrlX) is null by definition and
       the term is also omitted. We have either one, two or four terms.
         A node is a ground node if the queried unknown is the NULL pointer. */
    const tbv_unknownVariable_t * const pUnknownUCtrlPlus = tbv_getUnknownByNode
                                                                ( pTableOfVars
                                                                , pDevice->idxNodeCtrlPlus
                                                                )
                              , * const pUnknownUCtrlMinus = tbv_getUnknownByNode
                                                                ( pTableOfVars
                                                                , pDevice->idxNodeCtrlMinus
                                                                );

    const tbv_unknownVariable_t * const pUnknownUFrom = tbv_getUnknownByNode
                                                                ( pTableOfVars
                                                                , pDevice->idxNodeFrom
                                                                );
    if(pUnknownUFrom != NULL)
    {
        if(pUnknownUCtrlPlus != NULL)
        {
            coe_addAddend( &A[pUnknownUFrom->idxRow][pUnknownUCtrlPlus->idxCol]
                         , /* factor */          -1
                         , /* productOfConsts */ k
                         );
        }
        /* else: Node connected to "ctrlPlus" is a ground node. */

        if(pUnknownUCtrlMinus != NULL)
        {
            coe_addAddend( &A[pUnknownUFrom->idxRow][pUnknownUCtrlMinus->idxCol]
                         , /* factor */          +1
                         , /* productOfConsts */ k
                         );
        }
        /* else: Node connected to "ctrlMinus" is a ground node. */
    }
    /* else: Connected node is a ground node. */

    const tbv_unknownVariable_t * const pUnknownUTo = tbv_getUnknownByNode( pTableOfVars
                                                                          , pDevice->idxNodeTo
                                                                          );
    if(pUnknownUTo != NULL)
    {
        /* Same terms as for pUnknownUFrom but with inverse sign. */
        if(pUnknownUCtrlPlus != NULL)
        {
            coe_addAddend( &A[pUnknownUTo->idxRow][pUnknownUCtrlPlus->idxCol]
                         , /* factor */          +1
                         , /* productOfConsts */ k
                         );
        }
        /* else: Node connected to "ctrlPlus" is a ground node. */

        if(pUnknownUCtrlMinus != NULL)
        {
            coe_addAddend( &A[pUnknownUTo->idxRow][pUnknownUCtrlMinus->idxCol]
                         , /* factor */          -1
                         , /* productOfConsts */ k
                         );
        }
        /* else: Node connected to "ctrlMinus" is a ground node. */
    }
    /* else: Connected node is a ground node. */

} /* End of addSrcIByUConditions */




/**
 * Add those terms to the (half way completed) LES, that describe the conditions
 * superimposed by a current controlled current source.
 *   @param A
 * The matrix m*n of pointers to coefficients.
 *   @param pTableOfVars
 * The table of all constants, knowns and unknows, which are used in the LES by reference.
 *   @param idxDevice
 * The index of the device under progress. The equation and/or terms of equations for
 * this device are added to the LES.
 */

static void addSrcIByIConditions( coe_coefMatrix_t const A
                                , const tbv_tableOfVariables_t * const pTableOfVars
                                , unsigned int idxDevice
                                )
{
    assert(idxDevice < pTableOfVars->pCircuitNetList->noDevices);
    const pci_device_t * const pDevice = pTableOfVars->pCircuitNetList->pDeviceAry[idxDevice];
    assert(pDevice->type == pci_devType_srcIByI);

    /* For current sources we need to define the polarity: The (positive defined) current
       is flowing into the pin connected to node "from" node and it is flowing out of the
       source into node "to". This definition sounds inverse but is intuitive as the arrow
       in the symbol of the source has the orientation from "from" to "to". */

    /* The current controlled source refers to the current sensed by a current probe. We
       need to access the unknown current through this probe. */
    const tbv_unknownVariable_t * const pUnknownICtrl = tbv_getUnknownByDevice
                                                                ( pTableOfVars
                                                                , pDevice->idxCurrentProbe
                                                                );

    /* The current through the probe is multiplied with the device constant of the current
       controlled source. Access the constant. */
    const coe_productOfConst_t k = tbv_getConstantByDevice(pTableOfVars, idxDevice);

    /* Extend the current balances of the two connected nodes by a new term
       describing the current flowing through the current source.
         Don't do this, if the connected node is a ground node, i.e. if the returned
       unknown is the NULL pointer. */
    const tbv_unknownVariable_t * const pUnknownUFrom = tbv_getUnknownByNode
                                                                ( pTableOfVars
                                                                , pDevice->idxNodeFrom
                                                                );
    if(pUnknownUFrom != NULL)
    {
        /* factor: The current is flowing from the node the device's pin "from" is
           connected to back into the current source; this is by definition a negative
           contribution to the node's current balance. */
        coe_addAddend( &A[pUnknownUFrom->idxRow][pUnknownICtrl->idxCol]
                     , /* factor */          -1
                     , /* productOfConsts */ k
                     );
    }
    /* else: Connected node is a ground node. */

    const tbv_unknownVariable_t * const pUnknownUTo = tbv_getUnknownByNode( pTableOfVars
                                                                          , pDevice->idxNodeTo
                                                                          );
    if(pUnknownUTo != NULL)
    {
        /* factor: The current is flowing into the node the device's pin "to" is connected
           to; this is by definition a positive contribution to the node's current
           balance. */
        coe_addAddend( &A[pUnknownUTo->idxRow][pUnknownICtrl->idxCol]
                     , /* factor */          +1
                     , /* productOfConsts */ k
                     );
    }
    /* else: Connected node is a ground node. */

} /* End of addSrcIByIConditions */




/**
 * Add those terms to the (half way completed) LES, that describe the conditions
 * superimposed by a single device.
 *   @param A
 * The matrix m*n of pointers to coefficients. Prior to the first call of this routine the
 * matrix is initialized to all null coefficients (or NULL pointers).
 *   @param pTableOfVars
 * The table of all constants, knowns and unknows, which are used in the LES by reference.
 *   @param idxDev
 * The index of the device as defined in the circuit net list object found in the table *
 * \a pTableOfVars of symbols.
 */

static void addDeviceConditions( coe_coefMatrix_t const A
                               , const tbv_tableOfVariables_t * const pTableOfVars
                               , unsigned int idxDev
                               )
{
    assert(idxDev < pTableOfVars->pCircuitNetList->noDevices);
    switch(pTableOfVars->pCircuitNetList->pDeviceAry[idxDev]->type)
    {
    case pci_devType_srcU:
        addSrcUConditions(A, pTableOfVars, idxDev);
        break;

    case pci_devType_srcI:
        addSrcIConditions(A, pTableOfVars, idxDev);
        break;

    case pci_devType_conductance:
    case pci_devType_resistor:
    case pci_devType_capacitor:
    case pci_devType_inductivity:
        addPassiveDeviceConditions(A, pTableOfVars, idxDev);
        break;

    case pci_devType_opAmp:
        addOpAmpConditions(A, pTableOfVars, idxDev);
        break;

    case pci_devType_currentProbe:
        addCurrentProbeConditions(A, pTableOfVars, idxDev);
        break;

    case pci_devType_srcUByU:
        addSrcUByUConditions(A, pTableOfVars, idxDev);
        break;

    case pci_devType_srcUByI:
        addSrcUByIConditions(A, pTableOfVars, idxDev);
        break;

    case pci_devType_srcIByU:
        addSrcIByUConditions(A, pTableOfVars, idxDev);
        break;

    case pci_devType_srcIByI:
        addSrcIByIConditions(A, pTableOfVars, idxDev);
        break;

    default: assert(false); break;

    } /* End switch(Which kind of device?) */

} /* End of addDeviceConditions */




/**
 * Initialize the module at application startup.\n
 *   Mainly used to initialize globally accessible heap for LES coefficient objects.
 *   @param hGlobalLogger
 * This module will use the passed logger object for all reporting during application life
 * time.
 *   @remark
 * Do not forget to call the counterpart at application end.
 *   @see void les_shutdownModule()
 */

void les_initModule(log_hLogger_t hGlobalLogger)
{
    /* Use the passed logger during the module life time. */
    _log = log_cloneByReference(hGlobalLogger);

    /* Create a heap for all data types in use.
         Remark: Using our own memory management is not essential here. A network, which
       would ever make perfomance issues of C's malloc/free apparent is not imaginable.
       Using our own management is just it bit more handy with respect to releasing all
       memory after use. */
    _hMemMgrSubNet =  mem_createHeap( _log
                                    , /* Name */ "Network"
                                    , /* sizeOfDataObjects */   sizeof(subNetwork_t)
                                    , /* initialHeapSize */     10
                                    , /* allocationBlockSize */ 10
                                    );
#ifdef DEBUG
   subNetwork_t dummyObj;
   assert((char*)&dummyObj.pNext == (char*)&dummyObj + MEM_OFFSET_OF_LINK_POINTER
          &&  sizeof(dummyObj.pNext) == MEM_SIZE_OF_LINK_POINTER
         );
#endif
} /* End of les_initModule */




/**
 * Do all cleanup after use of the module, which is required to avoid memory leaks, orphaned
 * handles, etc.
 */

void les_shutdownModule()
{
    /* The DEBUG compilation looks for still allocated objects in order to detect memory
       leaks. */
#ifdef DEBUG
#define WARN_IF_UNFREED_MEM true
#else
#define WARN_IF_UNFREED_MEM false
#endif
    mem_deleteHeap(_hMemMgrSubNet, WARN_IF_UNFREED_MEM);
#undef WARN_IF_UNFREED_MEM

    _hMemMgrSubNet  = MEM_HANDLE_INVALID_HEAP;

    /* Invalidate the reference to the passed logger. It must no longer be used. */
    log_deleteLogger(_log);
    _log = LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT;

} /* End of les_shutdownModule */




/**
 * Create a linear equation system that describes the ideal behavior of the electric
 * circuit, which is decribed by a net list in the input file.\n
 *   The created LES object needs to be deleted after usage.
 *   @return
 * The input can contain a lot of logical errors, which inhibit the creation of a LES.
 * (There are many physical constraints, like the outputs of two op-amps, which must not be
 * directly connetced.) The function returns true if a LES could be created, false
 * otherwise.
 *   @param ppLES
 * The pointer to the created LES object is returned in * \a ppLES if the function
 * succeeds. Otherwise * \a ppLES will contain NULL.\n
 *   The created object contains the matrix of coefficients of the LES plus additional,
 * related information, e.g. about the (physical) constants refered to in the coefficients.
 * The returned object can be input to a symbolic solver.
 *   @param pCircuitNetList
 * The reference to the object that represents the parse result of the input file. It
 * mainly contains the net list of the electric circuit.
 *   @see void les_deleteLES(les_linearEquationSystem_t *)
 */

boolean les_createLES( les_linearEquationSystem_t * * const ppLES
                     , const pci_circuit_t * const pCircuitNetList
                     )
{
    /* Allocate matrix and initialize all coefficients to NULL. */
    les_linearEquationSystem_t *pLES = smalloc( sizeof(les_linearEquationSystem_t)
                                              , __FILE__
                                              , __LINE__
                                              );

    pLES->doWarn = true;
    pLES->A = NULL;

    /* Analyse the network topology expressed in the net list representing the circuit and
       transform it into a more useful data structure. */
    network_t *pNetwork = NULL;
    boolean success = analyseNetworkTopology(&pNetwork, pCircuitNetList);
    if(!success)
    {
        LOG_ERROR( _log
                 , "Input file doesn't specify a valid, computable electric circuit."
                   " Don't know what to do"
                 );
    }
    else if(pNetwork->noSubNets > 1)
    {
        /* We could make this better if we knew, whether the unconnected sub-graphs were
           at least logically interconnected, because one contains a source, which is
           controlled by quantities out of the other one. This information is basically
           available but is currently not extracted from the input. Instead, we make it
           informative if there's only at least one controlled source in use. */
        LOG_LOG( _log
               , pNetwork->hasControlledSources? log_info: log_warn
               , "Input file specifies %u unconnected graphs"
               , pNetwork->noSubNets
               );
    }

    /* pNetwork may be NULL in case of errors. */

    /* Count the devices, inspect their type and the type of a resulting linear equation
       and determine the sets of knows, unknows and constants. */
    if(success)
        success = determineReqVariables(&pLES->pTableOfVars, pNetwork);
    else
        pLES->pTableOfVars = NULL;

    deleteNetwork(pNetwork);

    /* Allocate space for the coefficients and reset them to null. */
    if(success)
    {
        const unsigned int noRows = pLES->pTableOfVars->noUnknowns
                         , noCols = pLES->pTableOfVars->noKnowns
                                    + pLES->pTableOfVars->noUnknowns;

        pLES->A = coe_createMatrix(noRows, noCols);
    }

    if(!success)
    {
        les_deleteLES(pLES);
        pLES = NULL;
    }

    /* Return the new, completed object (or NULL). */
    *ppLES = pLES;
    return success;

} /* End of les_createLES */




/**
 * Delete a linear equation system object. All memory is freed.
 *   @param pLES
 * The pointer to the deleted object. No action if this is NULL.
 *   @see boolean les_createLES(les_linearEquationSystem_t * * const, const pci_circuit_t *
 * const)
 */

void les_deleteLES(les_linearEquationSystem_t *pLES)
{
    if(pLES == NULL)
        return;

    if(pLES->pTableOfVars != NULL)
    {
        const unsigned int noRows = pLES->pTableOfVars->noUnknowns
                         , noCols = pLES->pTableOfVars->noKnowns
                                    + pLES->pTableOfVars->noUnknowns;
        coe_deleteMatrix(pLES->A, noRows, noCols);
        tbv_deleteTableOfVariables(pLES->pTableOfVars);
    }
    else
        assert(pLES->A == NULL);

    free(pLES);

} /* End of les_deleteLES */




/**
 * Get the number of symbols used in the LES: The knowns, unknowns and constants.
 *   @param pLES
 * The linear equation system object as got from a successful run of les_createLES.
 *   @param *pNoKnowns
 * The number of known variables is returned in * \a pNoKnowns.
 *   @param *pNoUnknowns
 * The number of unknown variables is returned in * \a pNoUnknowns.
 *   @param *pNoConstants
 * The number of (physical) constants is returned in * \a pNoConstants.
 */

void les_getNoVariables( const les_linearEquationSystem_t * const pLES
                       , unsigned int * const pNoKnowns
                       , unsigned int * const pNoUnknowns
                       , unsigned int * const pNoConstants
                       )
{
    *pNoKnowns    = pLES->pTableOfVars->noKnowns;
    *pNoUnknowns  = pLES->pTableOfVars->noUnknowns;
    *pNoConstants = pLES->pTableOfVars->noConstants;

} /* End of les_getNoVariables */




/**
 * Get access to the table of all symbols used in the LES or related to it: The knowns, the
 * unknowns and the (physical) constants, which appear in the coefficients.
 *   @return
 * A reference to the table of variables object.
 *   @param pLES
 * The linear equation system object as got from a successful run of les_createLES.
 */

const tbv_tableOfVariables_t *les_getTableOfVariables
                                        (const les_linearEquationSystem_t * const pLES)
{
    return pLES->pTableOfVars;

} /* End of les_getTableOfVariables */




/**
 * Get access to the table of all the unknowns of the LES.
 *   @return
 * A read-only reference to the table of unknowns. The returned pointer points into the
 * passed data structure * \a pLES; it's validity is identical to the life time of this
 * data structure.
 *   @param pLES
 * The linear equation system object as got from a successful run of les_createLES.
 *   @param pNoUnknowns
 * If not NULL, the number of unknowns is returned in * \a pNoUnknowns.
 *   @remark
 * The entries in the returned table are not in the unknowns' order of appearance in the
 * LES, see tbv_unknownVariable_t.idxCol. The order of unknowns in the returned table is
 * invariant during the life time of * \a pLES, but their order of appearance in the LES
 * is variant; it is influenced by two functions, which select a specific unknown for the
 * sover.
 *   @see boolean tbv_setTargetUnknownForSolver(tbv_tableOfVariables_t * const, const char
 * * const)
 *   @see boolean les_setupLES(les_linearEquationSystem_t * const, const char * const)
 */

const tbv_unknownVariable_t *les_getTableOfUnknowns
                                        ( const les_linearEquationSystem_t * const pLES
                                        , unsigned int * const pNoUnknowns
                                        )
{
    if(pNoUnknowns != NULL)
        *pNoUnknowns = pLES->pTableOfVars->noUnknowns;

    return pLES->pTableOfVars->unknownLookUpAry;


} /* End of les_getTableOfVariables */




/**
 * Setup the LES prior to running the solver.\n
 *   Currently, the solver is not capable to find a solution for all unknowns at once. As a
 * work around this routine forms the LES in a way that the solver will return the solution
 * of one specific, user specified unknown.\n
 *   This method can be called arbitrarily often on the same LES object after the object
 * has been successfully created. By calling this method and the solver m times, the full
 * solution can be figured out for a LES of size m times n.
 *   @return
 * The success. Run the solver only in case of true.
 *   @param pLES
 * The linear equation system object as got from a successful run of les_createLES.
 *   @param nameOfUnknown
 * The unknown, which the LES is to be solved for is identified by name.
 */

boolean les_setupLES( les_linearEquationSystem_t * const pLES
                    , const char * const nameOfUnknown
                    )
{
    assert(pLES != NULL  &&  pLES->pTableOfVars != NULL);

    /* Select the user demanded unknown for the solution.
         The order of unknowns has been determined at object creation time. This order
       depends on the input net list and the network analysis algorithm and has no specific
       meaning in terms of user desires. However, by simply exchanging two columns of the
       (later) LES on user demand, we can achieve that any unknown is represented in column
       m of the LES and this is the column, which the solver returns the fully figured out
       solution for. */
    boolean success = tbv_setTargetUnknownForSolver(pLES->pTableOfVars, nameOfUnknown);

    /* Compute all coefficients of the LES. */
    if(success)
    {
        assert(pLES->A != NULL);

        tbv_tableOfVariables_t * const pTableOfVars = pLES->pTableOfVars;
        const unsigned int noKnowns = pTableOfVars->noKnowns
                         , noUnknowns = pTableOfVars->noUnknowns
                         , noCols = noUnknowns + noKnowns;

        /* Free all previously used coefficients and reset them to null. */
        unsigned int m, n;
        for(m=0; m<noUnknowns; ++m)
        {
            for(n=0; n<noCols; ++n)
            {
                coe_freeCoef(pLES->A[m][n]);
                pLES->A[m][n] = coe_coefAddendNull();
            }
        }

        /* Create a temporarily used lookup table, which holds some properties of a node
           that is identified by its index. */
        const pci_circuit_t * const pCircuitNetList = pTableOfVars->pCircuitNetList;

        /* The LES will be setup several times, for each unknown to be solved. Suppress
           notifications about ground nodes the next time the LES is setup from the
           same object: It would always be the same, repeated information. */
        pLES->doWarn = false;

        unsigned int idxDev;
        for(idxDev=0; success && idxDev<pCircuitNetList->noDevices; ++idxDev)
            addDeviceConditions(pLES->A, pTableOfVars, idxDev);

        /* Double-check, that all coefficients are in the right order of their addends.
           The implementation of the solver depends on that. */
#ifdef DEBUG
        for(m=0; m<noUnknowns; ++m)
        {
            for(n=0; n<noCols; ++n)
                assert(coe_checkOrderOfAddends(pLES->A[m][n]));
        }
#endif

        /* Reporting on DEBUG level: Show all variables and coefficients. */
        if(log_checkLogLevel(_log, log_debug))
        {
            LOG_DEBUG(_log, "LES prior to elimination for unknown %s:", nameOfUnknown)
            tbv_logTableOfVariables(pTableOfVars);
            coe_logMatrix( log_debug
                         , pLES->A
                         , /* m */ pTableOfVars->noUnknowns
                         , /* n */ pTableOfVars->noKnowns + pTableOfVars->noUnknowns
                         , pTableOfVars
                         );
        }
    } /* End if(No error yet) */

    return success;

} /* End of les_setupLES */




