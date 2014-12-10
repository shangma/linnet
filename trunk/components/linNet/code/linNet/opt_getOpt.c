/**
 * @file opt_getOpt.c
 * Command line parser, using getopt.
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
 *   opt_parseCmdLine
 *   opt_echoUserInput
 * Local functions
 *   checkUserInput
 */

/*
 * Include files
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <assert.h>

#if OPT_USE_POSIX_GETOPT != 0
# include <unistd.h>
#else
# include <getopt.h>
#endif

#include "lin_linNet.h"
#include "opt_getOpt.h"


/*
 * Defines
 */

#if OPT_USE_POSIX_GETOPT != 0
# define HELP_TEXT                                                                          \
"usage: linNet [-hrsci] [-v <logLevel>] [-f <headerFormat>] [-l <logFileName>]"             \
" [-o <outputPath>] [--] {<circuitFileName>}\n"                                             \
"  h: Print this help and terminate\n"                                                      \
"  r: Print the software revision and terminate\n"                                          \
"  v: Verbosity; one out of INFO, RESULT, WARN, ERROR or FATAL. Default is RESULT\n"        \
"  f: Log entry format, one out of raw, short or long. Default is long\n"                   \
"  s: Silent. Do not echo results to stdout (but still write into log file)\n"              \
"  l: Log file name. Default is not to open a log file. Precondition: Either a log file\n"  \
"     is specified or -s is not given\n"                                                    \
"  c: Clear log file at the beginning. Default is to append\n"                              \
"  o: The path where to put the generated Octave code. The specified directory needs to\n"  \
"     exist. Default is not to generate Octave code\n"                                      \
"  i: Inhibit copying static Octave scripts. The generated Octave code builds on some\n"    \
"     common scripts, which are normally copied into the output folder. Use -i to\n"        \
"     not copy these files into each result\n"                                              \
"  {<circuitFileName>}: A list of circuit files, each either *.ckt or *.cir. At least\n"    \
"     one input file needs to be specified\n"
#else
# define HELP_TEXT                                                                          \
"usage: linNet [-hsci] [-v logLevel] [-f headerFormat] [-l[logFileName]] [-o[outputPath]]"  \
" [--] {circuitFileName}\n"                                                                 \
"Options:\n"                                                                                \
"  -h, --help\n"                                                                            \
"    Print this help and terminate\n"                                                       \
"  -r, --version\n"                                                                         \
"    Print the software revision and terminate\n"                                           \
"  -v LEVEL, --verbosity=LEVEL\n"                                                           \
"    Verbosity of application; LEVEL is one out of INFO, RESULT, WARN, ERROR or FATAL.\n"   \
"    Default is RESULT\n"                                                                   \
"  -f FORMAT, --format-of-log-entry=FORMAT\n"                                               \
"    Log entry format, FORMAT is one out of raw, short or long. Default is long\n"          \
"  -s, --silent\n"                                                                          \
"    Silent operation, only a greeting is emitted. Do not echo results to stdout (but\n"    \
"    still write into log file).\n"                                                         \
"      Precondition: Silent mode must be chosen only if a log file is in use\n"             \
"  -l[FILENAME], --log-file-name[=FILENAME]\n"                                              \
"    Log file name. No log file is opened if this option is not used. An appropriate\n"     \
"    file name is chosen by the application if the option is used without argument\n"       \
"    FILENAME.\n"                                                                           \
"      Precondition: Either using a log file is specified or -s and --silent are not\n"     \
"    given\n"                                                                               \
"  -c, --clear-log-file\n"                                                                  \
"    Clear the log file at the beginning of operation. Default is to append to a possibly\n"\
"    existing log file\n"                                                                   \
"  -o[DIRNAME], --Octave-output-directory[=DIRNAME]\n"                                      \
"    The path where to put the generated Octave code. The specified directory needs to\n"   \
"    exist. No Octave code is generated if this option is not used. The generated code\n"   \
"    is put into the current working directory if the option is used without argument\n"    \
"    DIRNAME\n"                                                                             \
"  -i, --do-not-copy-common-Octave-code\n"                                                  \
"    Inhibit copying static Octave scripts. The generated Octave code builds on some\n"     \
"    common scripts, which are normally copied into the output folder. Use -i in order\n"   \
"    to not copy these files into each result folder\n"                                     \
"Input files:\n"                                                                            \
"  Program arguments, which are neither options nor their arguments are considered input\n" \
"  files. Pass the names of the circuit files to be processed, each either a *.ckt or a\n"  \
"  *.cnl file. At least one input file needs to be specified\n"                             \
"    If the names of input files could clash with options or arguments then separate\n"     \
"  options and input files in this order by a double hyphen (--)\n"
#endif /* OPT_USE_POSIX_GETOPT */


/*
 * Local type definitions
 */


/*
 * Local prototypes
 */


/*
 * Data definitions
 */


/*
 * Function implementation
 */





/**
 * Double check the parsed user input. Some command line options might be mandatory but not
 * given or they might be contradictory. Print the help text if a problem is found and
 * return false.
 *   @return
 * \a true if everything is alright and the application can continue, \a false if a problem
 * would inhibit further processing.
 *   @param parseSuccess
 * A problem can have ocured already during parsing. If so, this function will immediately
 * print the help text and return.
 *   @param pCmdLineOptions
 * The command line parse result.
 */

static boolean checkUserInput( boolean parseSuccess
                             , const opt_cmdLineOptions_t * const pCmdLineOptions
                             )
{
    /* Don't do further checks and print error messages if the user just wants to get the
       help. */
    if(!pCmdLineOptions->help && !pCmdLineOptions->showVersion)
    {
        /* The command line option definitions basically allow to configure the application
           so that it wouldn't produce any output. This is useless and confusing. Lock this
           combination of options. */
        if(parseSuccess
           && !pCmdLineOptions->echoToConsole
           &&  pCmdLineOptions->logFileName == NULL
          )
        {
            parseSuccess = false;
            fprintf( stderr
                   , "Silent run (-s) and no log file (-l omitted) cannot be combined; no"
                     " output would be\n"
                     "produced at all, which is useless\n"
                   );
        }

        /* We need at least one file to process. */
        if(parseSuccess &&  pCmdLineOptions->noInputFiles == 0)
        {
            parseSuccess = false;
            fprintf( stderr
                   , "Circuit file is missing. Please, specify at least one input file\n"
                   );
        }
    } /* End if(User requested help?) */

    /* Print the help on demand or in case of errors. */
    if(!parseSuccess || pCmdLineOptions->help)
        fprintf(stdout, HELP_TEXT);

    /* Print the version string if requested. */
    if(parseSuccess && pCmdLineOptions->showVersion)
        fprintf(stdout, LIN_APP_NAME " revision " LIN_SW_REV "\n");

    /* Return whether the application can be continued: No in case of errors or if only the
       help text was requested. */
    return parseSuccess && !pCmdLineOptions->help && !pCmdLineOptions->showVersion;

} /* End of checkUserInput */




/**
 * Parse the command line. Depending on macro #OPT_USE_POSIX_GETOPT the implementation use
 * the basic POSIX command line parser getopt or the GNU extensions of this function. POSIX
 * only permits the short one character command line options. The GNU extensions support
 * long command line options and options with default arguments.
 *   @return
 * \a true if parsing succeeded. If help has been demanded or if an error occurs then an
 * error message including the help text is printed to stdout/stderr and \a false is
 * returned.
 *   @param pCmdLineOptions
 * The parse results are returned in * \a pCmdLineOptions.
 *   @param argc
 * The first argument of function main: The number of program arguments.
 *   @param argv
 * The second argument of function main: An array of \a argc constant strings, each a
 * program argument. An additional array entry NULL indicates the end of the list.
 */

boolean opt_parseCmdLine( opt_cmdLineOptions_t * const pCmdLineOptions
                        , signed int argc
                        , char *argv[]
                        )
{
    /* POSIX: Truncate function call; discard two addition arguments to match the call of
       the simpler getopt. */
#if OPT_USE_POSIX_GETOPT != 0
# define getopt_long(argc, argv, options, long_options, option_index)                       \
                getopt(argc, argv, options)
#endif

    /* The definition of the supported command line options. POSIX doesn't support the
       double colon to make the argument of a short option optional. */
#if OPT_USE_POSIX_GETOPT != 0
    const static char * const shortOptionString = "hrsciv:f:l:o:";
#else
    const char * const shortOptionString = "hrsciv:f:l::o::";
    const struct option longOptionAry[] =
    { {.name = "help", .has_arg = no_argument, .flag = NULL, .val = 'h'}
    , {.name = "version", .has_arg = no_argument, .flag = NULL, .val = 'r'}
    , {.name = "silent", .has_arg = no_argument, .flag = NULL, .val = 's'}
    , {.name = "clear-log-file", .has_arg = no_argument, .flag = NULL, .val = 'c'}
    , { .name = "do-not-copy-common-Octave-code"
      , .has_arg = no_argument
      , .flag = NULL
      , .val = 'i'
      }
    , {.name = "verbosity", .has_arg = required_argument, .flag = NULL, .val = 'v'}
    , {.name = "format-of-log-entry", .has_arg = required_argument, .flag = NULL, .val = 'f'}
    , {.name = "log-file-name", .has_arg = optional_argument, .flag = NULL, .val = 'l'}
    , { .name = "Octave-output-directory"
      , .has_arg = optional_argument
      , .flag = NULL
      , .val = 'o'
      }
    /* End of list: All null values */
    , {.name = NULL, .has_arg = 0, .flag = NULL, .val = 0}
    };
#endif /* OPT_USE_POSIX_GETOPT != 0 */

    pCmdLineOptions->help = false;
    pCmdLineOptions->showVersion = false;
    pCmdLineOptions->logLevel = NULL;
    pCmdLineOptions->logFileName = NULL; /* NULL means to not use a log file. */
    pCmdLineOptions->lineFormat = NULL;
    pCmdLineOptions->echoToConsole = true;
    pCmdLineOptions->doAppend = true;
    pCmdLineOptions->octaveOutputPath = NULL; /* NULL means to not generate Octave code. */
    pCmdLineOptions->dontCopyPrivateOctaveScripts = false;
    pCmdLineOptions->noInputFiles = 0;
    pCmdLineOptions->idxFirstInputFile = UINT_MAX;

    /* See libc.pdf, Chapter 25: The Basic Program/System Interface 651 and 653 */
    boolean success = true;
    opterr = 0; /* Global variable of getopt. */
    signed int c;
#if OPT_USE_POSIX_GETOPT == 0
    signed int idxLongOpt = 0;
#endif

    while(success
          &&  (c=getopt_long(argc, argv, shortOptionString, longOptionAry, &idxLongOpt)) != -1
         )
    {
        switch(c)
        {
        /* Help, no further operation */
        default:
            assert(false);
            success = false;
            break;

        /* Print help. */
        case 'h':
            pCmdLineOptions->help = true;
            break;

        /* Print software revision. */
        case 'r':
            pCmdLineOptions->showVersion = true;
            break;

        /* Level of verbosity. The levels are defined by and passed to module log. */
        case 'v':
            pCmdLineOptions->logLevel = optarg;
            break;

        /* Line header format. The formats are defined by and passed to module log. */
        case 'f':
            pCmdLineOptions->lineFormat = optarg;
            break;

        /* Silent: Logging into file but not echoing to stdout? */
        case 's':
           pCmdLineOptions->echoToConsole = false;
           break;

        /* The file name of the log file. */
        case 'l':
            /* The log file name has an optional argument. The default value is indicated
               by the empty string. */
            if(optarg != NULL)
                pCmdLineOptions->logFileName = optarg;
            else
            {
#if OPT_USE_POSIX_GETOPT != 0
                /* POSIX doesn't support optional arguments. */
                assert(false);
#endif
                pCmdLineOptions->logFileName = "";
            }
            break;

        /* Clear log: Do not append to existing log file. */
        case 'c':
           pCmdLineOptions->doAppend = false;
           break;

        /* The path where to place the the generated folders with Octave scripting. */
        case 'o':
            /* The option output path has an optional argument. The default value is
               indicated by the empty string. */
            if(optarg != NULL)
                pCmdLineOptions->octaveOutputPath = optarg;
            else
            {
#if OPT_USE_POSIX_GETOPT != 0
                /* POSIX doesn't support optional arguments. */
                assert(false);
#endif
                pCmdLineOptions->octaveOutputPath = "";
            }
            break;

        /* Inhibit copying common Octave scripts into each result. */
        case 'i':
            pCmdLineOptions->dontCopyPrivateOctaveScripts = true;
            break;

        /* Error handling: Check getopt's global variable optopt. */
        case '?':
            success = false;
            if(optopt == 'v')
            {
                fprintf( stderr
                       , "Option -%c requires a string out of INFO, RESULT, WARN,"
                         " ERROR or FATAL as argument\n"
                       , optopt
                       );
            }
            else if(optopt == 'f')
            {
                fprintf( stderr
                       , "Option -%c requires a string out of raw, short or long"
                         " as argument\n"
                       , optopt
                       );
            }
            else if(optopt == 'l')
            {
                fprintf( stderr
                       , "Option -%c requires a file name as argument\n"
                       , optopt
                       );
            }
            else if(optopt == 'o')
            {
                fprintf( stderr
                       , "Option -%c requires an existing target directory as argument\n"
                       , optopt
                       );
            }
            else if(isprint(optopt))
                fprintf(stderr, "Unknown option -%c\n", optopt);
            else
                fprintf(stderr, "Unknown option character 0x%x\n", (unsigned)optopt);
        }
    } /* End while(All program options) */

    if(optind < argc)
    {
        pCmdLineOptions->noInputFiles = argc - optind;
        pCmdLineOptions->idxFirstInputFile = optind;
    }

    /* The user input can contain contradictions and errors. */
    return checkUserInput(success, pCmdLineOptions);

#if OPT_USE_POSIX_GETOPT != 0
# undef getopt_long
#endif
} /* End of opt_parseCmdLine */




#ifdef DEBUG
/**
 * Print the parsed user input in a formatted way. Only the fileds of the interface struct
 * can be printed as the simple types they are defined but a true interpretation is not
 * possible. Therefore, this function will probably be useful for debugging purpose only.
 *   @param stream
 * An open stream from the stdlib to write in, pass e.g. stdout.
 *   @param pCmdLineOptions
 * The struct with all user input as got from \a opt_parseCmdLine. You can print the struct
 * even if the parser had reported a problem.
 */

void opt_echoUserInput(FILE *stream, const opt_cmdLineOptions_t * const pCmdLineOptions)
{
#define CHAR_PTR(s) ((s)!=NULL? (*(s)!='\0'? (s): "(default value)"): "NULL")
#define BOOL_STR(b) ((b)? "true": "false")

    fprintf( stream
           , "Help: %s\n"
             "Level of verbosity: %s\n"
             "Log entry format: %s\n"
             "Silent: %s\n"
             "Log file name: %s\n"
             "Clear log: %s\n"
             "Octave output path: %s\n"
             "Inhibit copying common Octave scripts: %s\n"
             "Number of input files: %u\n"
             "Index of first program file argument: %u\n"
           , BOOL_STR(pCmdLineOptions->help)
           , CHAR_PTR(pCmdLineOptions->logLevel)
           , CHAR_PTR(pCmdLineOptions->lineFormat)
           , BOOL_STR(!pCmdLineOptions->echoToConsole)
           , CHAR_PTR(pCmdLineOptions->logFileName)
           , BOOL_STR(!pCmdLineOptions->doAppend)
           , CHAR_PTR(pCmdLineOptions->octaveOutputPath)
           , BOOL_STR(pCmdLineOptions->dontCopyPrivateOctaveScripts)
           , pCmdLineOptions->noInputFiles
           , pCmdLineOptions->idxFirstInputFile
           );

#undef CHAR_PTR
#undef BOOL_STR
} /* End of opt_echoUserInput */
#endif /* DEBUG */



