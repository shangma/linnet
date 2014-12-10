#ifndef MEM_MEMORYMANAGER_INCLUDED
#define MEM_MEMORYMANAGER_INCLUDED
/**
 * @file mem_memoryManager.h
 * Definition of global interface of module mem_memoryManager.c
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


/*
 * Defines
 */

/** The invalid heap handle. Should be used for initialization of any handle variable. */
#define MEM_HANDLE_INVALID_HEAP NULL

/** Contract with client: The handled data objects need to respect room for a data object
    linking pointer. The reserved space needs to begin at this offset inside each of the
    allocated memory blocks. */
#define MEM_OFFSET_OF_LINK_POINTER  (0)

/** Contract with client: The handled data objects need to respect room for a data object
    linking pointer inside each of the allocated memory blocks. The reserved space has this
    number of bytes. */
#define MEM_SIZE_OF_LINK_POINTER    (sizeof(void*))


/*
 * Global type definitions
 */

/** A heap is implemented as an opaque type. */
struct mem_heap_t;

/** The clients of the memory manager use handles to heaps for allocation and freeing
    memory. */
typedef struct mem_heap_t *mem_hHeap_t; 


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */

/** Initialize a heap for specific (list) elements. */
mem_hHeap_t mem_createHeap( log_hLogger_t hLogger
                          , const char *name
                          , size_t sizeOfDataObjects
                          , unsigned int initialHeapSize
                          , unsigned int allocationBlockSize
                          );

/** Allocate a single element on the heap. */
void *mem_malloc(mem_hHeap_t hHeap);

/** Allcoate a linked list of n elements on the heap. */
void *mem_mallocList(mem_hHeap_t hHeap, unsigned int lenOfList);

/** Free a single data object. */
void mem_free(mem_hHeap_t hHeap, void *pDataObj);

/** Free a linked list of elements. */
void mem_freeList(mem_hHeap_t hHeap, void *pHeadElement);

/** Destroy the heap, release all memory. */
unsigned long mem_deleteHeap(mem_hHeap_t hHeap, boolean warnIfUnfreed);

#endif  /* MEM_MEMORYMANAGER_INCLUDED */
