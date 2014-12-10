#ifndef TYPES_INCLUDED
#define TYPES_INCLUDED
/**
 * @file types.h
 * Definition of global, basic types.
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


/*
 * Defines
 */
 

/*
 * Global type definitions
 */

/** Definition of Boolean values.
      @remark The definition of \a boolean needs to become unsigned char if using Windows
    stuff, otherwise the definition is incompatible with the header file windows.h. */
typedef enum {FALSE = (1==0), TRUE = (1==1), false = (1==0), true = (1==1)} boolean;


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */



#endif  /* TYPES_INCLUDED */
