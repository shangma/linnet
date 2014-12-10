/**
 * @file fio_fifoChar.c
 * 
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
 *   fio_createFifoChar
 *   fio_deleteFifoChar
 *   fio_writeChar
 *   fio_getNoElements
 *   fio_readChar
 * Local functions
 *   mallocMemChunk
 *   freeMemChunk
 */

/*
 * Include files
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "smalloc.h"
#include "fio_fifoChar.h"


/*
 * Defines
 */
 
 
/*
 * Local type definitions
 */
 
 
/*
 * Local prototypes
 */

/** A memory chunk of the FIFO. While the FIFO grows an increasing number of these blocks
    form a linked list. */
typedef struct fifoBlock_t
{
    /** The link to the next chunk of memory or NULL. */
    struct fifoBlock_t *pNext;
    
    /** The next chunk of memory. */
    char *elementAry;

} fifoBlock_t;


/** the index to an element of the FIFO. */
typedef struct fifoIdx_t
{
    /** The pointer to the memory chunk we look into. */
    fifoBlock_t *pChunk;
    
    /** The linear element index in the current chunk. */
    unsigned int idx;

} fifoIdx_t;


/** The FIFO data structure. A character can be stored and retrieved later. */
typedef struct fio_fifoChar_t
{
    /** An option: The size in which new memory chunks are allocated if the FIFO becomes
        too small. */
    unsigned int blockSize;
    
    /** The number of stored elements. */
    unsigned int noElements;
    
    /** The read index. */
    fifoIdx_t idxRead;
    
    /** The write index. */
    fifoIdx_t idxWrite;
    
} fio_fifoChar_t;



/*
 * Data definitions
 */
 
 
/*
 * Function implementation
 */


/**
 * Allocate a new FIFO memory chunk.
 *   @return
 * Get a pointer to the new allocated chunk. This chunk is initialized as an empty block,
 * that can be appended just like that to the list of chunks in the FIFO object.
 *   @param hFifo
 * The handle of the FIFO object.
 */ 

static fifoBlock_t *mallocMemChunk(fio_hFifoChar_t hFifo)
{
    fifoBlock_t *pChunk = smalloc(sizeof(fifoBlock_t), __FILE__, __LINE__);
    pChunk->pNext = NULL;
    pChunk->elementAry = smalloc( hFifo->blockSize * sizeof(*pChunk->elementAry)
                                , __FILE__
                                , __LINE__
                                );
    return pChunk;
    
} /* End of mallocMemChunk */




/**
 * Free a FIFO memory chunk that had been allocated using fifoBlock_t
 * *mallocMemChunk(fio_hFifoChar_t).
 *   @param pBlock
 * The pointer to the memeory chunk to be deleted.
 */ 

static void freeMemChunk(fifoBlock_t *pBlock)
{
    free(pBlock->elementAry);
    free(pBlock);

} /* End of freeMemChunk */




/**
 * Create a new FIFO object.
 *   @return
 * Get the handle of the new object.
 *   @param blockSize
 * The size of a single, malloc allocated chunk of memory. Whenever more elements are
 * stored as this number a new chunk is allocated to enlarge the FIFO. Must not be null.
 */

fio_hFifoChar_t fio_createFifoChar(unsigned int blockSize)
{
    assert(blockSize > 0);
    fio_fifoChar_t *pFifo = smalloc(sizeof(fio_fifoChar_t), __FILE__, __LINE__);
    
    pFifo->blockSize = blockSize;
    pFifo->noElements = 0;

    pFifo->idxRead.pChunk = NULL;
    pFifo->idxRead.idx = 0;
    
    pFifo->idxWrite.pChunk = NULL;
    pFifo->idxWrite.idx = 0;

    return pFifo;

} /* End of fio_createFifoChar */





/**
 * Delete an object as created with fio_hFifoChar_t fio_createFifoChar(unsigned int) after use.
 *   @param hFifo
 * The handle of the object to delete.
 */

void fio_deleteFifoChar(fio_hFifoChar_t hFifo)
{
    /* Iterate over all chunks of memory and free them. */    
    fifoBlock_t *pFifoBlock = hFifo->idxRead.pChunk;
    while(pFifoBlock != NULL)
    {
        assert(pFifoBlock->pNext != NULL  ||  pFifoBlock == hFifo->idxWrite.pChunk);
        fifoBlock_t *pFifoBlockToFree = pFifoBlock;
        pFifoBlock = pFifoBlockToFree->pNext;
        freeMemChunk(pFifoBlockToFree);
    }
    
    /* Free the object itself. */
    free(hFifo);

} /* End of fio_deleteFifoChar */





/**
 * Add a new element to the end of the queue.
 *   @param hFifo
 * The handle of the FIFO object.
 *   @param ch
 * The stored element.
 */

void fio_writeChar(fio_hFifoChar_t hFifo, char ch)
{
    /* An empty FIFO requires the first, new chunk of memory. */
    if(hFifo->idxWrite.pChunk == NULL)
    {
        assert(hFifo->noElements == 0  &&  hFifo->idxWrite.idx == 0
               &&  hFifo->idxRead.pChunk == NULL  &&  hFifo->idxRead.idx == 0
               &&  hFifo->blockSize > 0
              );

        fifoBlock_t *pFirstChunk = mallocMemChunk(hFifo);

        hFifo->idxRead.pChunk = pFirstChunk;
        hFifo->idxWrite.pChunk = pFirstChunk;
    }
    else if(hFifo->idxWrite.idx == hFifo->blockSize)
    {
        /* We need another chunk of memory, the previous one is full. */
        assert(hFifo->idxWrite.pChunk->pNext == NULL);
        hFifo->idxWrite.pChunk->pNext = mallocMemChunk(hFifo);
        hFifo->idxWrite.pChunk = hFifo->idxWrite.pChunk->pNext;
        hFifo->idxWrite.idx = 0;
    }
    
    hFifo->idxWrite.pChunk->elementAry[hFifo->idxWrite.idx++] = ch;
    ++ hFifo->noElements;
    
} /* End of fio_writeChar */





/**
 * Get the number of elements stored in the FIFO.
 *   @return
 * Get the number.
 *   @param hFifo
 * The handle of the FIFO object.
 */

unsigned int fio_getNoElements(fio_hFifoChar_t hFifo)
{
    return hFifo->noElements;
    
} /* End of fio_getNoElements */




/**
 * Read (and remove) an element from the head of the queue.\n
 *   This method must not be used on an empty FIFO.
 *   @return
 * Get the read element.
 *   @param hFifo
 * The handle of the FIFO object.
 *   @see unsigned int fio_getNoElements(fio_hFifoChar_t)
 */

char fio_readChar(fio_hFifoChar_t hFifo)
{
    assert(hFifo->noElements > 0  &&  hFifo->idxRead.pChunk != NULL);    
    if(hFifo->noElements == 0)
        return (char)-1;

    /* Do we read beyond the boundary of one chunk to the next one? */
    if(hFifo->idxRead.idx == hFifo->blockSize)
    {
        /* Move read pointer to the next chunk in the list and free the current one. */
        assert(hFifo->idxRead.pChunk->pNext != NULL
               &&  hFifo->idxRead.pChunk != hFifo->idxWrite.pChunk
              );
              
        fifoBlock_t *pFifoBlockToFree = hFifo->idxRead.pChunk;
        hFifo->idxRead.pChunk = pFifoBlockToFree->pNext;
        hFifo->idxRead.idx = 0;
        freeMemChunk(pFifoBlockToFree);
    }
    
    -- hFifo->noElements;
    return hFifo->idxRead.pChunk->elementAry[hFifo->idxRead.idx++];

} /* End of fio_readChar */


