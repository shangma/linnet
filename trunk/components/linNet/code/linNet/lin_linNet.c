/**
 * @file lin_linNet.c
 *   Main function of linNet.
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
 *   main
 * Local functions
 *   getTimeStr
 *   getLogFileName
 *   makeOctaveOutputDir
 *   processInputFile
 */

/*
 * Include files
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>

#include "log_logger.h"
#include "smalloc.h"
#include "fil_file.h"
#include "mem_memoryManager.h"
#include "rat_rationalNumber.h"
#include "pci_parserCircuit.h"
#include "tbv_tableOfVariables.h"
#include "coe_coefficient.h"
#include "les_linearEquationSystem.h"
#include "frq_freqDomainSolution.h"
#include "msc_mScript.h"
#include "sol_solver.h"
#include "opt_getOpt.h"
#include "lin_linNet.h"


/*
 * Defines
 */

/** The path separator character as a string constant. */
#ifdef __WINNT__
# define SL "\\"
#else
# define SL "/"
#endif


/*
 * Local type definitions
 */


/*
 * Local prototypes
 */


/*
 * Data definitions
 */

/** The path to the application directory as malloc allocated string if known or NULL if
    unknown. */
const char *_installDir = NULL;


/*
 * Function implementation
 */



/**
 * Get the current system time as printable string.
 *   @return
 * The time string is returned. The returned pointer is valid until next entry into this
 * function.
 *   @remark
 * The function is not reentrant but uses static memory to store the time information.
 */

static const char *getTimeStr()
{
    /* Get current time as string of fixed length. */
    time_t t;
    time(&t);
    const char * const timeStrLF = ctime(&t);

    /* Copy the time string only partial, we don't need the line feed at the end. */
    assert(strlen(timeStrLF) == 25);
    static char timeStr[25];
    timeStr[0] = '\0';
    strncat(timeStr, timeStrLF, sizeof(timeStr)-1);

    return timeStr;

} /* End of getTimeStr */




/**
 * Figure out the name of the log file. It is either user-specified or derived from a circuit
 * file or application name.\n
 *   If a single input circuit file is processed then log file is named as the circuit file
 * and it is placed into to output folder.\n
 *   If several circuit files are processed then the common log file is placed into the
 * output folder. Its name is derived from the application name.
 *   @return
 * Get the file name as malloc allocated string. Or NULL if the user had specified: "don't
 * use a log file at all."
 *   @param pCmdLine
 * The result of the command line parsing is passed by reference.
 *   @param circuitFileName
 * The name of the only or the first input file name.
 */

static const char *getLogFileName( const opt_cmdLineOptions_t * const pCmdLine
                                 , const char * const circuitFileName
                                 )
{
    if(pCmdLine->logFileName == NULL)
        return NULL;
    else if(*pCmdLine->logFileName != '\0')
        return stralloccpy(pCmdLine->logFileName);
    else
    {
        /* None trivial case: Determine the default name. */

        /* The default name of the Octave output path should have been resolved prior to
           calling this function. */
        assert(pCmdLine->octaveOutputPath == NULL  ||  *pCmdLine->octaveOutputPath != '\0');

        /* If the user demands a specific output path then place the log file into that
           path. Otherwise use the current working directory. */
        const char *path;
        if(pCmdLine->octaveOutputPath != NULL)
            path = pCmdLine->octaveOutputPath;
        else
            path = ".";

        if(pCmdLine->noInputFiles > 1)
        {
            /* If there are several input files, then we use the application name for the
               log file. */
            char logFileName[strlen(path) + sizeof(SL) + sizeof(LIN_LOG_FILE_NAME)];
            snprintf(logFileName, sizeof(logFileName), "%s" SL LIN_LOG_FILE_NAME, path);
            return stralloccpy(logFileName);
        }
        else
        {
            /* We have a single input file. Its name is re-used for the log file. */
            assert(pCmdLine->noInputFiles == 1);
            char *pureFileName;
            fil_splitPath( /* pPath */ NULL
                         , &pureFileName
                         , /* pExt */ NULL
                         , circuitFileName
                         );

            char logFileName[strlen(path) + sizeof(SL) + strlen(pureFileName)
                             + sizeof(LIN_LOG_FILE_NAME_EXT)
                            ];
            snprintf( logFileName
                    , sizeof(logFileName)
                    , "%s" SL "%s%s"
                    , path
                    , pureFileName
                    , LIN_LOG_FILE_NAME_EXT
                    );
            free((char*)pureFileName);
            return stralloccpy(logFileName);

        } /* End if(One or multiple input files?) */

    } /* End if(Trivial situation or do we have to figure out the name?) */

} /* End of getLogFileName */




/**
 * Check if the needed output directory for the generated Octave scripts already exists. If
 * not, make it now and pre-fill it with all the static, never changing parts of the
 * Octave script code.
 *   @return
 * \a true if operation succeeded, otherwise \a false.
 *   @param outputPath
 * The location of the output folder (i.e. its intended parent directory), not including
 * the final slash.
 *   @param folderName
 * The name of the folder that holds all required scripting.
 *   @param dontCopyPrivateScripts
 * Normally, the folder is self-contained, i.e. it contains the cicuit depending, generated
 * script representing the computation result plus some static, always-same scripts needed
 * to run the generated script code. This means that any result will contain the always
 * same support scripts. The user may decide to install this code in his Octave environment
 * (by coying the files and using Octave's addpath to address to the copy), in which case
 * he doesn't need these scripts over and over again. This flag can be set to \a true in
 * order to not copy these files.
 *   @param hLog
 * Success and problem reports are made in this logger.
 */

static boolean makeOctaveOutputDir( const char * const outputPath
                                  , const char * const folderName
                                  , boolean dontCopyPrivateScripts
                                  , log_hLogger_t hLog
                                  )
{
    /* Compose the full path to the folder to be made. */
    char octavePath[strlen(outputPath) + sizeof(SL) + strlen(folderName)];
    snprintf(octavePath, sizeof(octavePath), "%s" SL "%s", outputPath, folderName);

    /* Check if the aimed folder already exists: If so we do not force copying again (e.g.
       to restore the static part of the scripting). It is a wanted use case that someone
       corrects or extends this part of the scripting and we should not discard his
       modifications. */
    DIR *hDir = opendir(octavePath);
    if(hDir != NULL)
    {
        closedir(hDir);

        /* The next message needs to be done on lowest level: If we have more than a
           single result definition for one and the same circuit file then we will
           necessarily see this message for result 2, 3, ... */
        LOG_DEBUG( hLog
                 , "Octave output folder %s already exists. The input related parts"
                   " of the Octave code are re-generated and existing files are overwritten."
                   " The static, problem independent parts of the code are not re-generated"
                 , octavePath
                 );

        /* An already existing target folder is not considered a fault, we simply reuse
           that one and overwrite the generated, dynamic part of the Octave scripting. */
        return true;
    }

    /* Create the Octave folder at the aimed output path. */
    boolean success = mkdir( octavePath
#ifdef __unix__
                           , /* mode: full access to anybody */ S_IRWXO | S_IRWXG | S_IRWXU
#endif
                           ) == 0;
    if(!success)
    {
        LOG_ERROR( hLog
                 , "Can't create the output directory %s (errno: %d, %s). No Octave script"
                   " code can be generated. Please check existence of parent directory and"
                   " check the access rights"
                 , octavePath
                 , errno
                 , strerror(errno)
                 );
    }

    /* Copy the code template folder into the aimed, new result directory. */
    if(success && !dontCopyPrivateScripts)
    {
        if(_installDir != NULL)
        {
            char resourceFolder[strlen(_installDir)
                                + sizeof(LIN_OCTAVE_CODE_TEMPLATE_FOLDER_NAME)
                                + sizeof(SL)
                               ];
            snprintf( resourceFolder
                    , sizeof(resourceFolder)
                    , "%s" SL LIN_OCTAVE_CODE_TEMPLATE_FOLDER_NAME
                    , _installDir
                    );

            if(fil_copyDir(octavePath, resourceFolder))
            {
                LOG_DEBUG( hLog
                         , "Template folder %s with Octave code successfully copied to"
                           " the aimed target location %s"
                         , resourceFolder
                         , octavePath
                         );
            }
            else
            {
                LOG_ERROR( hLog
                         , "Can't copy template folder %s with Octave code to the aimed"
                           " target location %s. The generated Octave scripts won't"
                           " be usable. Please check for conflicting file and folder"
                           " names and check access rights"
                         , resourceFolder
                         , octavePath
                         );
            }
        }
        else
        {
            LOG_ERROR( hLog
                     , "Application installation directory is unknown; important program"
                       " resources can't be accessed. The generated Octave scripts won't"
                       " be usable. Please set environment variable " LIN_ENV_VAR_HOME
                     )
        }

        /* We don't set the overall success to false if copying the static parts of the
           Octave script fails: The application can still generate the circuit related
           parts of the code for investigation; however, these parts of the code won't be
           executable. */
    }

    return success;

} /* End of makeOctaveOutputDir */




/**
 * A single input file is completely processed: The cicuit file is parsed, the computation
 * is conducted, the results are presented and the Octace scripts are generated.
 *   @return
 * \a true if operation entirely succeeded, \a false if errors occured, which invalidate
 * the computed output.
 *   @param circuitFileName
 * The name of the input file.
 *   @param octaveOutputPath
 * NULL or a path designation. If not NULL then Octave scripts are generated in the
 * specified path which represent the found solution and which permit to do some (numeric)
 * analysis of the circuit.
 *   @param dontCopyPrivateOctaveScripts
 * If Octave scripts should be generated: The actually generated, computation result
 * related scripts depend on some static, always same support scripts. If \a
 * dontCopyPrivateOctaveScripts is \a true then these files are added to the result. One
 * might e.g. have installed these files once as part of his Octave installation, so that
 * making a copy for each computation result would be counterproductive.
 *   @param hLog
 * The logger to write all progress messages into.
 *   @see
 * uint8_t otherFunction(uint16_t)
 *   @remark
 */

static boolean processInputFile( const char * const circuitFileName
                               , const char * const octaveOutputPath
                               , boolean dontCopyPrivateOctaveScripts
                               , log_hLogger_t hLog
                               )
{
    /* Parse the input file and generate a net list object. */
    const pci_circuit_t *pParseResult = NULL;
    boolean success = pci_parseCircuitFile( hLog
                                          , &pParseResult
                                          , circuitFileName
                                          );

    /* Create a linear equation system object from the parse result. */
    les_linearEquationSystem_t *pLES = NULL;
    if(success)
        success = les_createLES(&pLES, pParseResult);

    /* Compute the solution of the LES. */
    const sol_solution_t *pSolution = NULL;
    if(success)
        success = sol_createSolution(&pSolution, pLES);

    /* Delete the LES, which is solved and no longer needed. */
    les_deleteLES(pLES);
    pLES = NULL;

    /* Print the algebraic solution of the LES. This is not yet the wanted, final result
       representation and therefore it is done only on level INFO. */
    if(success)
        sol_logSolution(pSolution, /* logLevel */ log_info);

    signed int idxResult;
    if(success)
    {
        for(idxResult=-1; idxResult<(signed)pParseResult->noResultDefs; ++idxResult)
        {
            /* Actually, a result with index -1 doesn't exist as such. -1 means the generic
               result of all full solutions, for each of the unknowns of the LES, whichever
               these are. It is demanded only if the user didn't specify any other result. */
            if(idxResult == -1)
            {
                if(pParseResult->noResultDefs == 0)
                {
                    LOG_WARN( hLog
                            , "No user-defined result found in input file. The solution for"
                              " all dependent quantities is figured out instead. This generic"
                              " result can become very bulky"
                            );
                    log_flush(hLog);
                }
                else
                    continue;
            }

            /* Generate the requested result from the algebraic solution of the LES. */
            const frq_freqDomainSolution_t *pFreqDomainSolution;
            boolean successResult = frq_createFreqDomainSolution( &pFreqDomainSolution
                                                                , pSolution
                                                                , idxResult
                                                                );

            /* Print the solution of the LES in the frequency domain. */
            if(successResult)
            {
                frq_logFreqDomainSolution( pFreqDomainSolution
                                         , hLog
                                         , /* logLevel */ log_result
                                         );
            }

            /* If Octave scripts are wanted as result representation: Compose the name of
               the Octave output file and open the file. */
            if(successResult &&  octaveOutputPath != NULL)
            {
                /* The name of the folder collecting all parts of the Ocatve script code is
                   derived from the name of the circuit file. */
                char *folderName;
                fil_splitPath( NULL
                             , &folderName
                             , NULL
                             , circuitFileName
                             );

                /* The generated scripts are always collected in a folder. The user has
                   specified the name of this folder. Create it now and fill it with the
                   static part of the Octave scripts if it shouldn't exist yet. */
                successResult = makeOctaveOutputDir( octaveOutputPath
                                                   , folderName
                                                   , dontCopyPrivateOctaveScripts
                                                   , hLog
                                                   );
                if(successResult)
                {
                    /* The name of the result specific script file is derived from the name
                       of the user-defined result. */
                    msc_mScript_t *pMScript;
                    char octaveFileName[strlen(octaveOutputPath)
                                        + strlen(folderName)
                                        + strlen(pFreqDomainSolution->name)
                                        + sizeof(SL SL ".m")
                                       ];
                    snprintf( octaveFileName
                            , sizeof(octaveFileName)
                            , "%s" SL "%s" SL "%s.m"
                            , octaveOutputPath
                            , folderName
                            , pFreqDomainSolution->name
                            );
                    if(msc_createMScript( &pMScript
                                        , octaveFileName
                                        , circuitFileName
                                        , pFreqDomainSolution->name
                                        )
                      )
                    {
                        if(!msc_writeTextBlock(pMScript, msc_txtBlkHeader))
                            successResult = false;
                        if(!msc_writeTextBlock(pMScript, msc_txtBlkLoadPkgs))
                            successResult = false;
                        if(!frq_exportAsMCode(pFreqDomainSolution, pMScript))
                            successResult = false;
                        if(!msc_writeTextBlock(pMScript, msc_txtBlkTrailer))
                            successResult = false;
                    }

                    /* The M script is now generated, the object is no longer used. */
                    msc_deleteMScript(pMScript);
                }

                free(folderName);

            } /* End if(User demands Octave scripts?) */

            /* The frequency domain result is not longer referenced. Delete it. */
            frq_deleteFreqDomainSolution(pFreqDomainSolution);

            /* Propagate success for this result computation to the global result. */
            if(!successResult)
                success = false;

        } /* End for(All user defined results) */

    } /* End if(Algebraic solution is available?) */

    /* Delete the reference to the parse result. */
    pci_deleteParseResult(pParseResult);
    pParseResult = NULL;

    /* The algebraic solution of the LES is no longer needed. */
    sol_deleteSolution(pSolution);
    pSolution = NULL;

    return success;

} /* End of processInputFile */




/**
 * Main entry point of the application. Parse input file, conduct the computation, present
 * the results.
 *   @return
 * \a 0 if application succeeded or -1 if it reported an error.
 *   @param argc
 * The number of program arguments.
 *   @param argv
 * An array of \a argc constant strings, each a program argument. An additional array entry
 * NULL indicates the end of the list.
 */

signed int main(signed int argc, char *argv[])
{
    const char * const greeting =
           "-----------------------------------------------------------------------------\n"
           " " LIN_APP_NAME
                    " - The Software for symbolic Analysis of linear Electronic Circuits\n"
           " Copyright (C) 2014 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)\n"
           " This is free software; see the source for copying conditions. There is NO\n"
           " warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"
           "-----------------------------------------------------------------------------";
    
    /* Print the greeting, which may disregard the switch -s, "be silent". Doing command
       line parsing first (to have the knowledge whether -s is given) would however mean,
       that all feedback to the command line interface (including output of the help text)
       would appear before the greeting - which is worse. */
    printf("%s\n", greeting);

    /* Parse the command line. Errors and problems are reported to both stdout and stderr. */
    opt_cmdLineOptions_t cmdLine;
    boolean success = opt_parseCmdLine(&cmdLine, argc, argv);

#ifdef DEBUG
    //opt_echoUserInput(stdout, &cmdLine);
#endif
    if(!success)
    {
        /* Don't consider it an error if the user explicitly demands some information only. */
        return (cmdLine.help || cmdLine.showVersion)? 0: -1;
    }

    /* Some command line options have optional arguments. If the arguments have been
       omitted then find approriate default values now. */
    if(cmdLine.octaveOutputPath != NULL  &&  *cmdLine.octaveOutputPath == '\0')
        cmdLine.octaveOutputPath = "."; /* Operate in current working directory. */
    const char *logFileName = getLogFileName(&cmdLine, argv[cmdLine.idxFirstInputFile]);

    log_initModule();

    /* Create the logger object. */
    boolean cantOpenLogFile = false;
    log_hLogger_t hGlobalLogger = LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT;
    if(!log_createLogger( &hGlobalLogger
                        , logFileName
                        , log_result
                        , log_fmtLong
                        , cmdLine.echoToConsole
                        , cmdLine.doAppend
                        )
      )
    {
        /* The constructor of log only fails if a log file had tried to be opened. */
        assert(logFileName != NULL);

        /* The logger object can still be used if it at least logs to stdout. */
        if(cmdLine.echoToConsole)
        {
            /* Print the error message later, when we have completed the configuration of
               the logger object. */
            cantOpenLogFile = true;
        }
        else
        {
            fprintf( stderr
                   , "Can't open log file %s. Application is terminated as no results"
                     " could be reported\n"
                   , logFileName
                   );
            return -1;
        }
    } /* End if(Could the log file be opened?) */

    /* If we get here: The logger object can be used even if it failed to open the log file
       since it echos everything to stdout. We proceed. */

    /* Configure the logger with the demands of the command line. */
    if(cmdLine.lineFormat != NULL)
        log_parseLineFormat(hGlobalLogger, cmdLine.lineFormat);
    if(cmdLine.logLevel != NULL)
        log_parseLogLevel(hGlobalLogger, cmdLine.logLevel);

    /* Log the greeting but don't do this a second time on the normal console. */
    log_setEchoToConsole(hGlobalLogger, /* echoToConsole */ false);
    LOG_RESULT(hGlobalLogger, "\n%s", greeting)
    log_setEchoToConsole(hGlobalLogger, cmdLine.echoToConsole);

    if(logFileName != NULL)
        LOG_INFO(hGlobalLogger, "Log file name is %s", logFileName)
    if(cantOpenLogFile)
        LOG_ERROR(hGlobalLogger, "Can't open log file %s", logFileName)

    /* The log file name is not used down here. */
    if(logFileName != NULL)
    {
        free((char*)logFileName);
        logFileName = NULL;
    }

    /* Try to locate the application's resource files; look for the installation directory.
       If it can be determined then the ptah is returned as malloc allocated string. */
    assert(argc > 0);
    _installDir = fil_findApplication(argv[0]);
    if(_installDir != NULL)
        LOG_DEBUG(hGlobalLogger, "Application installation directory is %s", _installDir)
    else
    {
        LOG_ERROR( hGlobalLogger
                 , "Can't locate the application's installation directory. Please set"
                   " environment variable " LIN_ENV_VAR_HOME ", which should hold the"
                   " name of the directory, where the executable file of this application"
                   " is located"
                 )
    }

    /* If the logger object doesn't write a line header with full time information we make
       a time notice now so that the sub-sequent lines of the log can be related to the
       absolute time. */
    if(log_getLineFormat(hGlobalLogger) != log_fmtLong)
        LOG_RESULT(hGlobalLogger, "Beginning of processing at %s", getTimeStr())

    /* Initialize the modules. */
    pci_initModule();
    rat_initModule(hGlobalLogger);
    coe_initModule(hGlobalLogger);
    tbv_initModule(hGlobalLogger);
    les_initModule(hGlobalLogger);
    sol_initModule(hGlobalLogger);
    frq_initModule(hGlobalLogger);
    msc_initModule(hGlobalLogger);

    /* Loop over all named input file. Continue even in case of failures; all circuit files
       should be independent of eachother. */
    unsigned int u, noSuccessfulFiles;
    for(u=cmdLine.idxFirstInputFile, noSuccessfulFiles=0; u<(unsigned)argc; ++u)
    {
        if(processInputFile( /* circuitFileName */ argv[u]
                           , cmdLine.octaveOutputPath
                           , cmdLine.dontCopyPrivateOctaveScripts
                           , hGlobalLogger
                           )
          )
        {
            /* Count successful files and report a statistics if there are more than one
               input files. */
            ++ noSuccessfulFiles;
        }
        else
            success = false;

    } /* End for(All input files on the command line) */

    if(cmdLine.noInputFiles > 1)
    {
        if(noSuccessfulFiles == cmdLine.noInputFiles)
        {
            LOG_INFO( hGlobalLogger
                    , "Successfully processed all %u input files"
                    , cmdLine.noInputFiles
                    )
        }
        else
        {
            LOG_WARN( hGlobalLogger
                    , "Successfully processed %u out of %u input files"
                    , noSuccessfulFiles
                    , cmdLine.noInputFiles
                    )
        }
    }

    /* Final cleanup by the modules. Particularly useful in DEBUG compilation as memory
       leaks can be recognized. */
    msc_shutdownModule();
    frq_shutdownModule();
    sol_shutdownModule();
    les_shutdownModule();
    tbv_shutdownModule();
    coe_shutdownModule();
    rat_shutdownModule();
    pci_shutdownModule();

    /* If the logger object doesn't write a line header with full time information we make
       a final time notice now. */
    if(log_getLineFormat(hGlobalLogger) != log_fmtLong)
        LOG_RESULT(hGlobalLogger, "End of processing at %s", getTimeStr());

    /* Delete global application log and shutdown module log. */
    log_deleteLogger(hGlobalLogger);
    log_shutdownModule();

    if(_installDir != NULL)
        free((char*)_installDir);

    return success? 0: -1;

} /* End of main */
