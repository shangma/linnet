#ifndef MSC_M_SCRIPT_INCLUDED
#define MSC_M_SCRIPT_INCLUDED
/**
 * @file msc_mScript.h
 * Definition of global interface of module msc_mScript.c
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

/** Indication, that this is not a valid object. Used instead of the pointer to a true
    object. */
#define MSC_NULL_FILE   NULL


/*
 * Global type definitions
 */

/** The M script object. */
typedef struct msc_mScript_t
{
    /** A counter of references to this object. Used to control deletion of object. */
    unsigned int noReferencesToThis;
    
    /** The file name. */
    const char *fileName;
    
    /** The name of the circuit file, which this M script is generated for. */
    const char *circuitFileName;
    
    /** The name of the user-defined result, which is modelled in this M script. */
    const char *resultName;
    
    /** The file handle. */
    FILE *hFile;
    
#ifdef DEBUG
    /** Is the file handle borrowed by a client of the object? In which case not output is
        possible. */
    boolean handleBorrowed;
#endif
} msc_mScript_t;


/** Some predefined text patterns can be used to model the contents of the generated M
    script. Here's an enumeration of the known text elements. */
typedef enum
{
    /** The file header, copyright notice, license, etc. */
    msc_txtBlkHeader
    
    /** The cosing elements of the file. */
    , msc_txtBlkTrailer
    
    /** A blank line. */
    , msc_txtBlkBlankLine

    /** A command to add all required folders to the Octave search path. */
    , msc_txtBlkAddPath
    
    /** A command to load all required Octave packages. */
    , msc_txtBlkLoadPkgs
    
} msc_enumTextBlock_t;
             
             
/*
 * Global data declarations
 */


/*
 * Global inline interface
 */

/**
 * Get the file name of the generated Octave script.
 *   @return
 * Get the name as a pointer of same life time as * \a pMSCript.
 *   @param pMScript
 * The Octave script object.
 */
 
static inline const char *getFileName(const msc_mScript_t * const pMScript)
    { return pMScript->fileName; }


/*
 * Global prototypes
 */

/** Initialize the module prior to first use of any of its methods or global data objects. */
void msc_initModule(log_hLogger_t hGlobalLogger);

/** Shutdown of module after use. Release of memory, closing files, etc. */
void msc_shutdownModule(void);

/** Create a new M script object. */
boolean msc_createMScript( msc_mScript_t ** const ppMScript
                         , const char * const fileName
                         , const char * const circuitFileName
                         , const char * const resultName
                         );

/** Get another reference to the same object. */
msc_mScript_t *msc_cloneByReference(msc_mScript_t * const pExistingObj);

/** Delete a  or a reference to it after use. */
void msc_deleteMScript(msc_mScript_t * const pMScript);

/** Write a particular, predefined part of the generated M script. */
boolean msc_writeTextBlock( msc_mScript_t * const pMScript
                          , msc_enumTextBlock_t kindOfTextBlock
                          );
                          
/** Borrow the stream object from the M script object to do direct output. */
FILE *msc_borrowStream(msc_mScript_t * const pMScript);

/** Notify that the borrowed stream is no longer used for direct output. */
void msc_releaseStream(msc_mScript_t * const pMScript);

#endif  /* MSC_M_SCRIPT_INCLUDED */
