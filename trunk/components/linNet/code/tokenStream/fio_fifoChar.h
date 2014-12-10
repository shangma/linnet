#ifndef FIO_FIFOCHAR_INCLUDED
#define FIO_FIFOCHAR_INCLUDED
/**
 * @file fio_fifoChar.h
 * Definition of global interface of module fio_fifoChar.c
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

/*
 * Include files
 */


/*
 * Defines
 */


/*
 * Global type definitions
 */

/** The FIFO data structure. A character can be stored and retrieved later. */
typedef struct fio_fifoChar_t *fio_hFifoChar_t;


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */

/** Create a new FIFO object. */
fio_hFifoChar_t fio_createFifoChar(unsigned int blockSize);

/** Delete an object as created with fio_hFifoChar_t fio_createFifoChar(unsigned int) after
    use. */
void fio_deleteFifoChar(fio_hFifoChar_t hFifo);

/** Add a new element to the end of the queue. */
void fio_writeChar(fio_hFifoChar_t hFifo, char ch);

/** Get the number of elements currently stored in the FIFO. */
unsigned int fio_getNoElements(fio_hFifoChar_t hFifo);

/** Read (and remove) an element from the head of the queue. */
char fio_readChar(fio_hFifoChar_t hFifo);

#endif  /* FIO_FIFOCHAR_INCLUDED */
