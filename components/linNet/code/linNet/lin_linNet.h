#ifndef LIN_LINNET_INCLUDED
#define LIN_LINNET_INCLUDED
/**
 * @file lin_linNet.h
 * Definition of global interface of module lin_linNet.c
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

/** The application name. */
#define LIN_APP_NAME    "linNet"

/** The revision. */
#define LIN_SW_REV      "1.0.1"

/** The name of the application's executable file. */
#ifdef __WINNT__
# define LIN_APP_FILE_NAME  LIN_APP_NAME ".exe"
#else
# define LIN_APP_FILE_NAME  LIN_APP_NAME
#endif

/** The extension for names of log files. */
#define LIN_LOG_FILE_NAME_EXT   ".log"
/** The name of the application's default log file name. */
#define LIN_LOG_FILE_NAME       LIN_APP_NAME LIN_LOG_FILE_NAME_EXT


/** The name of the environment variable that should point to the installation directory of
    this application. */
#define LIN_ENV_VAR_HOME    "LINNET_HOME"

/** The name of the template folder holding the static parts of the generated Octave script
    code. This folder is copied into the output folder as part of code generation. */
#define LIN_OCTAVE_CODE_TEMPLATE_FOLDER_NAME    "private"


/*
 * Global type definitions
 */


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */

/** Main function of application. */
signed int main(signed int argc, char *argv[]);


#endif  /* LIN_LINNET_INCLUDED */
