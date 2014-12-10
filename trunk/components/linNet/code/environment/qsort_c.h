#ifndef QSORT_C_INCLUDED
#define QSORT_C_INCLUDED
/**
 * @file qsort_c.h
 * Definition of global interface of module qsort_c.c
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

#include <sys/types.h>


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
 * Global prototypes
 */

/** Quick sort algorithm with context information passed into the compare function. */
extern void qsort_c( void *a
                   , size_t n
                   , size_t es
                   , int (*cmp)(const void *, const void *, const void * context)
                   , const void * context
                   );


#endif  /* QSORT_C_INCLUDED */
