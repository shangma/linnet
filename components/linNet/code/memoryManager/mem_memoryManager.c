/**
 * @file mem_memoryManager.c
 *   This module implements a simple but very fast and stable memory management. It uses
 * the free-list algorithm: A pre-defined number of memory blocks is organized as a linked
 * list. Requested elements are taken from the head of the list and returned elements are
 * added in front of this list. The algorithm is free of fragmentation or variable run-time
 * issues but can be applied to elements of same size only. Therefore, the interface
 * permitts to create the memory management as an object. An application may create such an
 * object for each type of object, where the high performance of this implementation in
 * comparison to the general purpose heap malloc/free is essential.\n
 *   To further increase the performance for applictions, which heavily deal with linked
 * lists, it has decided to make the free-list structure identical to the client's list
 * structure, i.e. the element linking pointer is the same inside the memory management and
 * at the application side. The element handling can't be done typesafe in C as a memory
 * manager can be instatiated for data objects of different data types. Taking both
 * considerations into repect an important constraint on the handled data objects can be
 * derived:\n
 *   The handled data objects must reserve the space for a pointer at the base address of
 * the handled data objects. This pointer is used by the memory management to link the free
 * elements in the heap.\n
 *   However, a client, who wants to implement linked lists using the memory management
 * may use this pointer for its own list-linking purpose. In this case he can request a
 * linked list of n elements from the memory management at once and he can free a linked
 * list of data objects at once.\n
 *   All handling of data objects in this module is done typeless and based on void
 * pointers. Therefore a run-time type check is strongly recommended. Please add some code
 * like
 * @code
 *   #include <assert.h>
 *
 *   typedef struct mydataObj_t {
 *      mydataObj_s *myListLink; // This needs to be the first member!
 *      ... myOtherDataObjMembers;
 *   } myDataObj_t;
 *
 *   int main() {
 *   #ifdef DEBUG
 *      myDataObj_t dummyObj;
 *      assert((char*)&dummyObj.myListLink == (char*)&dummyObj + MEM_OFFSET_OF_LINK_POINTER
 *             &&  sizeof(dummyObj.myListLink) == MEM_SIZE_OF_LINK_POINTER
 *            );
 *   #endif
 *   }
 * @endcode
 * to your application. If your application doesn't operate on linked lists, you'd probably
 * change the definition of member myListLink to something like void *pReserved. The
 * suggested self-test of the code should still be present.\n
 *   The memory manager allocates the memory, which it partitions and manages in linked
 * lists of data objects, in large chunks from the general purpose heap using malloc. A new
 * chunk is allocated whenever the free list is exhausted. These chunks are not freed again
 * unless you delete the complete memory manager.\n
 *   There's no error handling strategy. As long as the general purpose heap provides
 * memory the allocation of data objects or lists of such will never fail, but if there's
 * no system memory left, the error handling simply is the abortion of the application
 * after writing an error message to stderr.
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
/* Module interface
 *   mem_createHeap
 *   mem_deleteHeap
 *   mem_malloc
 *   mem_mallocList
 *   mem_free
 *   mem_freeList
 * Local functions
 *   debug_checkConsistency
 *   partitionHeadChunk
 *   allocNewChunk
 */

/*
 * Include files
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "types.h"
#include "smalloc.h"
#include "log_logger.h"
#include "mem_memoryManager.h"


/*
 * Defines
 */

/** The implemented algorithm handles all actual data elements by void*. Specify an
    alignment, which will be suitable for possible actual elements, which are going to be
    stored in the allocated memory blocks. */
#define COMMON_MACHINE_ALIGNMENT   4

/** The data elements the heap is partitioned in basically have the size of the client's
    data objects, but they are enlarged so that sub-sequent elements will all be on an
    aligned address. */
#define SIZEOF_ALIGNED(s) (((s)+(COMMON_MACHINE_ALIGNMENT)-1) / (COMMON_MACHINE_ALIGNMENT)  \
                           * (COMMON_MACHINE_ALIGNMENT)                                     \
                          )


/*
 * Local type definitions
 */
 
 
/*
 * Local prototypes
 */
 
/** The definition of a single chunk of heap memory. */
typedef struct _memChunk_t
{
    /** All chunks form a NULL terminated linked list of chunks. */
    struct _memChunk_t *pNext;
    
    /** The client available heap memory. */
    void *pHeapMem;
   
} memChunk_t;


/** The definition of a memory manager object. */
typedef struct mem_heap_t
{
    /* The name of the heap for logging purpose. */
    const char *name;
    
    /* An optional logger object for activity reporting. */
    log_hLogger_t hLogger;

    /* The head of the list of allocated memory chunks. */
    memChunk_t *pHeadOfChunkList;
    
    /* A pointer to the last allocated memory chunk. Used for fast appending of new chunk. */
    memChunk_t *pTailOfChunkList;
    
    /* A pointer to the head of the free list. */
    void *pHeadOfFreeList;
    
    /* The size of a single data object in Byte. */
    size_t sizeOfObj;
    
    /* The size of the heap in number of data objects. */
    unsigned long sizeOfHeap;
    
    /* The number of data objects still available in the free list. */
    unsigned long noFreeObjs;

    /* The size of each but the initial memory chunk in number of managed data objects. */
    unsigned int sizeOfChunk;
    
} heap_t;


/*
 * Data definitions
 */
 
 
/*
 * Function implementation
 */

#ifdef DEBUG
/**
 * Self-test function: Check consistency of heap data structure. The test result is
 * reported by assertion. The code is used and available only for debug compilation.
 *   @param pHeap
 * The pointer to the heap under test.
 */ 

static void debug_checkConsistency(heap_t *pHeap)
{
    /* Self-test: Check consistency of free list, count elements. */
    unsigned long idxFreeObj;
    void *pNext = pHeap->pHeadOfFreeList;
    assert((pHeap->noFreeObjs > 0  &&  pNext != NULL)
           ||  (pHeap->noFreeObjs == 0  &&  pNext == NULL)
          );
    for(idxFreeObj=0; idxFreeObj<pHeap->noFreeObjs; ++idxFreeObj)
    {
        pNext = *(void**)pNext;
        assert((idxFreeObj+1<pHeap->noFreeObjs  &&  pNext != NULL)
               ||  (idxFreeObj+1 == pHeap->noFreeObjs  &&  pNext == NULL)
              );
    }
} /* End of debug_checkConsistency */
#endif




/**
 * Allocate a new chunk to the (exhausted) heap and partition its for allocation request by
 * the heap's client.
 *   @param pHeap
 * The pointer to the heap under progress.
 *   @param noDataObjs
 * The size of the new chunk in number of data objects.
 */ 

static void allocNewChunk(heap_t * const pHeap, unsigned int noDataObjs)
{
    if(pHeap->hLogger != LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT)
    {
        LOG_DEBUG( pHeap->hLogger
                 , "mem:allocNewChunk: Allocating another chunk of %u objects for heap"
                   " %s. Total heap size now: %lu Byte"
                 , noDataObjs
                 , pHeap->name
                 , (pHeap->sizeOfHeap + noDataObjs)*(unsigned long)pHeap->sizeOfObj
                 )
        log_flush(pHeap->hLogger);
    }

    /* Allocate the new memory chunk. */
    memChunk_t * const pNewChunk = smalloc(sizeof(memChunk_t), __FILE__, __LINE__);
    pNewChunk->pHeapMem = smalloc(pHeap->sizeOfObj*noDataObjs, __FILE__, __LINE__);

    /* The new chunk becomes the new head of the list of chunks. */
    pNewChunk->pNext = pHeap->pHeadOfChunkList;
    
    /* The initial chunk is both, the head and the tail of the initial list of chunks. At
       run-time, the tail pointer doesn't need an update any more. */
    if(pHeap->pTailOfChunkList == NULL)
    {
        assert(pHeap->pHeadOfChunkList == NULL);
        pHeap->pTailOfChunkList = pNewChunk;
    }
    pHeap->pHeadOfChunkList = pNewChunk;
    
    /* Partition the new chunk. */ 
    assert(noDataObjs >= 1);
    char *pDataObj = pNewChunk->pHeapMem;
    unsigned int u;
    for(u=0; u<noDataObjs-1; ++u)
    {
        char *pAddrOfNextDataObj = pDataObj + pHeap->sizeOfObj;
        *(void**)pDataObj = (void*)pAddrOfNextDataObj;
        pDataObj = pAddrOfNextDataObj;
    }
    /* pDataObj points to the link pointer in the last data object. We write the current
       head of the free list into this pointer - the new chunk becomes the head of the fee
       list. */
    *(void**)pDataObj = pHeap->pHeadOfFreeList;
    pHeap->pHeadOfFreeList = pNewChunk->pHeapMem;
    
    /* Record new memory. */
    pHeap->sizeOfHeap += noDataObjs;
    pHeap->noFreeObjs += noDataObjs;
    
} /* End of allocNewChunk */




/*
 * Initialize a heap for (list) elements of a specific data type.
 *   @return
 * The handle to the new heap is returned. Use this handle to allocate data objects.
 *   @param hLogger
 * If not LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT, the passed logger is used to report about
 * major heap activities.
 *   @param name
 * Used for logging purpose only: The name of the created heap.
 *   @param sizeOfDataObjects
 * All elements managed with the new heap have the same type and thus size. The size is
 * passed.  Needs to be greater of equal to sizeof(void*).
 *   @param initialHeapSize
 * The size of the first, initial chunk of memory, which is managed as heap of data
 * objects, as number of data objects. Needs to be greater of equal to 1.
 *   @param allocationBlockSize
 * The size of the subsequent chunks of memory, which will be allocated in case to enlarge
 * the heap of data objects, as number of data objects. Needs to be greater of equal to 1.
 */

mem_hHeap_t mem_createHeap( log_hLogger_t hLogger
                          , const char *name
                          , size_t sizeOfDataObjects
                          , unsigned int initialHeapSize
                          , unsigned int allocationBlockSize
                          )
{
    assert(sizeOfDataObjects >= sizeof(void*) &&  initialHeapSize >= 1
           &&  allocationBlockSize >= 1  && name != NULL
          );

    /* Create the heap object itself. */
    heap_t *pHeap = smalloc(sizeof(heap_t), __FILE__, __LINE__);
    
    /* Logging: logger object and name of heap are needed. */
    pHeap->hLogger = log_cloneByReference(hLogger);
    pHeap->name = name;
    
    /* Run-time constants: The size of all handled data objects and the number of data
       objects to re-allocate when heap is empty. */
    pHeap->sizeOfObj = SIZEOF_ALIGNED(sizeOfDataObjects);
    pHeap->sizeOfChunk = allocationBlockSize;
    
    /* No chunk at the moment, empty free list, size null. */
    pHeap->pHeadOfChunkList = NULL;
    pHeap->pTailOfChunkList = NULL;
    pHeap->pHeadOfFreeList = NULL;
    pHeap->sizeOfHeap = 0;
    pHeap->noFreeObjs = 0;
    
    if(hLogger != LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT)
    {
        LOG_DEBUG( hLogger
                 , "mem_createHeap: Create new heap %s, client data object size %u"
                   " Byte, effectively %u Byte"
                 , name
                 , sizeOfDataObjects
                 , pHeap->sizeOfObj
                 )
    }
    
    /* Partition the new chunk. We use the same function as at run-time. */ 
    allocNewChunk(pHeap, initialHeapSize);
    
#ifdef DEBUG
    debug_checkConsistency(pHeap);
#endif
    return pHeap;

} /* End of mem_createHeap. */





/**
 * Delete a heap. All references to currently allocated data objects become invalid and
 * must no longer be used.
 *   @return
 * The number of still allocated data objects is returned, i.e. data objects on the heap,
 * which had not been freed before deletion of heap. Normally it is an error in the client
 * application if this value is not equal to zero.
 *   @param pHeap
 * The handle to the heap as got from mem_createHeap.
 *   @param warnIfUnfreedMem
 * If true, a warning is written to the logger if un-freed elements exit, i.e. when the
 * function returns a non null value.
 */

unsigned long mem_deleteHeap(heap_t *pHeap, boolean warnIfUnfreedMem)
{
    assert(pHeap != NULL);    
    
#ifdef DEBUG
    debug_checkConsistency(pHeap);
#endif

    /* Loop over all memory chunks. */
    unsigned int noChunks = 0;
    memChunk_t *pChunk = pHeap->pHeadOfChunkList;
    assert(pChunk != NULL);
    do
    {
        memChunk_t *pNextChunk = pChunk->pNext;
        assert( (pNextChunk != NULL  &&  pChunk != pHeap->pTailOfChunkList)
                ||  (pNextChunk == NULL  &&  pChunk == pHeap->pTailOfChunkList)
              );
              
        free(pChunk->pHeapMem);
        free(pChunk);
        ++ noChunks;
        pChunk = pNextChunk;
    }
    while(pChunk != NULL);
    
    assert(pHeap->noFreeObjs <= pHeap->sizeOfHeap);
    unsigned long noUnfreedObjs = pHeap->sizeOfHeap - pHeap->noFreeObjs;
    
    if(pHeap->hLogger != LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT)
    {
        LOG_DEBUG( pHeap->hLogger
                 , "mem_deleteHeap: Delete heap %s, with a total size of %lu data"
                   " objects (%lu Byte) in %u chunks of memory"
                 , pHeap->name
                 , pHeap->sizeOfHeap
                 , pHeap->sizeOfHeap * (unsigned long)pHeap->sizeOfObj
                   + sizeof(memChunk_t) * noChunks + sizeof(heap_t)
                 , noChunks
                 )
        if(warnIfUnfreedMem &&  noUnfreedObjs > 0)
        {
            LOG_WARN( pHeap->hLogger
                     , "mem_deleteHeap: Heap %s still has %lu un-freed data objects at"
                       " instance of deletion"
                     , pHeap->name
                     , noUnfreedObjs
                     )
        }
        
        /* Discard reference to the logger object. */
        log_deleteLogger(pHeap->hLogger);

    } /* if(Do we have to do reporting?) */
    
    free(pHeap);    
    
    return noUnfreedObjs;
    
} /* End of mem_deleteHeap */




/**
 * Allocate a single data object on the heap.
 *   @return
 * The pointer to the data object.
 *   @param pHeap
 * The handle to the heap as got from mem_createHeap.
 */

void *mem_malloc(heap_t *pHeap)
{
    
    if(pHeap->noFreeObjs < 1)
        allocNewChunk(pHeap, pHeap->sizeOfChunk);
    
    assert(pHeap->noFreeObjs >= 1);
    -- pHeap->noFreeObjs;
    void *pDataObj = pHeap->pHeadOfFreeList;
    pHeap->pHeadOfFreeList = *(void**)pDataObj;
    *(void**)pDataObj = NULL;

    return pDataObj;
    
} /* End of mem_malloc */




/**
 * Allocate a linked list of data objects on the heap. This operation is useful and defined
 * only if the client knows and respects that the first bytes of the allocated data objects
 * contain a pointer to the next allocated data object and so on. The client can access
 * the sequence of returned, allocated data objects only this way, he must not consider the
 * returned data an array of n elements!
 *   @return
 * The pointer to the head of the returned list, i.e. to the first data object in the list.
 *   @param pHeap
 * The handle to the heap as got from mem_createHeap.
 *   @param lenOfList
 * The length of the list, i.e. the number of linked data objects. lenOfList is greater
 * than 1.
 *   @remark
 * The client may disregard and overwrite the link pointer in the first data bytes of each
 * returned data element if he's not going to return the list or parts of it with the
 * counterpart function void *mem_freeList(heap_t *), but if he instead will return each
 * element separately using the function void *mem_free(heap_t *).
 */

void *mem_mallocList(heap_t *pHeap, unsigned int lenOfList)
{
    assert(lenOfList > 0);
    
    while(pHeap->noFreeObjs < lenOfList)
        allocNewChunk(pHeap, pHeap->sizeOfChunk);

    pHeap->noFreeObjs -= lenOfList;
    
    /* We will return the head of the free list. */
    void *pDataObj = pHeap->pHeadOfFreeList;
    
    /* We will cut the free list after lenOfList data objects. The pointer to the new head
       of free list is found as next pointer of the lenOfList-1 data object in the current
       free list. After evaluating it we have to set the next pointer of this data element
       to NULL: It becomes the tail element of the returned list. */
    unsigned int u;
    void *pNext = pDataObj;
    for(u=1; u<lenOfList; ++u)
        pNext = *(void**)pNext;
    pHeap->pHeadOfFreeList = *(void**)pNext;
    *(void**)pNext = NULL;

    return pDataObj;
    
} /* End of mem_mallocList */




/**
 * Return a no longer used data object to the heap it was allocated on before.
 *   @param pHeap
 * The handle to the heap the data object belongs to.
 *   @param pDataObj
 * The pointer to the data object, which is returned to the heap.
 */

void mem_free(heap_t *pHeap, void *pDataObj)
{
    assert(pDataObj != NULL  &&  pHeap->sizeOfHeap > pHeap->noFreeObjs);
    
    ++ pHeap->noFreeObjs;
    *(void**)pDataObj = pHeap->pHeadOfFreeList;
    pHeap->pHeadOfFreeList = pDataObj;
    
} /* End of mem_free */




/**
 * Return a linked list of no longer used data objects to the heap they were allocated on
 * before.\n
 *   Caution, this function will work only if the client is fully aware of the data object
 * link pointer in the first bytes of each data object and if he respects and uses the
 * pointers in this way. All returned objects have to be linked by this pointer and the
 * link pointer of the last one needs to be NULL.
 *   @param pHeap
 * The handle to the heap the data objects belongs to.
 *   @param pHeadOfList
 * The pointer to the head of the list that is returned to the heap, i.e. the first one of
 * the linked data objects.
 */

void mem_freeList(heap_t *pHeap, void *pHeadOfList)
{
    assert(pHeadOfList != NULL  &&  pHeap->sizeOfHeap > pHeap->noFreeObjs);
    
    /* We iterate along the returned list in order to determine its length and to find its
       tail element. This element is needed for concatenation of the returned list with the
       current free list. */
    unsigned long u = 1;
    void *pNext = pHeadOfList;
    while(*(void**)pNext != NULL)
    {
        ++ u;
        assert(u + pHeap->noFreeObjs <= pHeap->sizeOfHeap);
        pNext = *(void**)pNext;
    }
    
    /* pNext now points to the last returned element, identified by a link pointer of NULL. */
    pHeap->noFreeObjs += u;
    *(void**)pNext = pHeap->pHeadOfFreeList;
    pHeap->pHeadOfFreeList = pHeadOfList;
    
} /* End of mem_freeList */




