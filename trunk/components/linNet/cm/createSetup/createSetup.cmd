@echo off
:: Get all files which are relevant for the end user from the SVN repository and pack them
:: into an archive for shipping. See usage message at file end for more details.

if "%1" == "" goto LUsage
if "%1" == "--help" goto LUsage
if "%1" == "-h" goto LUsage
if "%1" == "-?" goto LUsage
if "%1" == "/?" goto LUsage
if "%1" == "/h" goto LUsage

setlocal

:: Check prerequisites for successful run of this script.
if "%TEMP%" == "" (
    echo To run this script the environment variable TEMP needs to be set properly.
    echo TEMP needs to point to a directory with write access for temporarily used files.
    goto :eof
)
:: Availability of SVN command line tool.
svn --version > nul
if ERRORLEVEL 1 (
    echo The command line tool SVN could not be found. This may happen if you use SVN
    echo solely via its GUI client Tortoise. Please install the SVN command line tool and
    echo restart this script.
    goto :eof
)

:: Calculate time information and use it to make file and folder names unique.
set now=%date%
set timeInfo=%now:~-2%%now:~-7,2%%now:~-10,2%
set now=%time%
set timeInfo=%timeInfo%%now:~0,2%%now:~3,2%%now:~6,2%%now:~9,2%
set tmpCheckoutFolder=%TEMP%\linNet-%timeInfo%

:: Evaluate the command line. The revision number is optional.
::   A special, reserved revision designation is "trunk" which refers to the head
:: revision of the software trunk. This choice is very helpful for maintenance and
:: test work on the export scripting itself. Same for "branches".
if /I "%1" == "trunk" (
    set rev=
    set tag=trunk
    set url=trunk
    
) else ( if /I "%1" == "branches" (
    set rev=
    set tag=%2
    set url=branches/%2

) else ( :: Normal situation: We reference a tag.
    if "%2" == "" (
        set rev=
        set tag=%1
        set url=tags/%1
    ) else (
        set rev=-r %1
        set tag=%2
        set url=tags/%2
    )
) )


:: The file name of the aimed setup is tagged with the version information.
if "%2" == "" (
    set setupFileNameWithSrcs=linNet-%tag%.zip
    set setupFileNameBinaries=linNet-binaries-%tag%.zip
) else (
    set setupFileNameWithSrcs=linNet-%tag%-r-%1.zip
    set setupFileNameBinaries=linNet-binaries-%tag%-r-%1.zip
)

set url=https://linnet.googlecode.com/svn/%url%
:: echo rev=%rev%, url=%url%
:: echo setup with sources=%setupFileNameWithSrcs%
:: echo setup with binaries only=%setupFileNameBinaries%
:: goto :eof

:: Get the wanted version, put it into the TEMP dir. Then go there and run the
:: setup-creating script there: this way, the setup creation process really fits to the
:: wanted version of the file set. Do not use the current working version of this script!
svn checkout %rev% %url% "%tmpCheckoutFolder%"
if ERRORLEVEL 1 (
    echo Setup could not be created. SVN returned an error. Please inspect the screen output for details.
    goto :eof
)

:: Go to the directory in the got file where the setup creation will take place.
pushd "%tmpCheckoutFolder%\components\linNet\cm\createSetup"

:: Create a version description.
pushd "%tmpCheckoutFolder%\components\linNet"
echo Generating version file from SVN information
SubWCRev.exe . cm\createSetup\template_version.txt cm\createSetup\version.txt -f
popd

:: Complete the version description.
echo Archive Compilation Date:>> version.txt
date /T>> version.txt
time /T>> version.txt
if /I not "%rev%" == "" (
    echo Source: %url%, %rev%>> version.txt
) else (
    echo Source: %url%>> version.txt
)

:: Build the software in all Windows configurations from the got source files. (The
:: binaries for the other systems need to be uploaded in SVN and can be copied only.)
::   An external script is applied although it just contains a few, simple commands: As the
:: other script is part of the exported revision, these commands can become subject to
:: changes in the life cycle of the software (as opposed to this script which must not
:: undergo any essential changes).
call buildAll.cmd

:: Create the dependent documentary files which are not under source control but which need
:: to be in the setup. An external script is applied although it just contains a few,
:: simple commands: As the other script is part of the exported revision, these commands
:: can become subject to changes in the life cycle of the software (as opposed to this
:: script which must not undergo any essential changes).
call makeDoc.cmd

:: Now create the archives by running the file copying script. The executed scripts are the
:: ones got from SVN, thus those which define the file set required in the exported
:: software version (as opposed to in the current head of the repository).
call copyAndZipFiles.withSources.cmd %setupFileNameWithSrcs%
if ERRORLEVEL 1 (
    echo Setup with sources could not be created. copyAndZipFiles.withSources
    echo returned an error. Please inspect the screen output for details.
    popd
    goto :eof
)
call copyAndZipFiles.binaries.cmd %setupFileNameBinaries%
if ERRORLEVEL 1 (
    echo Setup with binary files could not be created. copyAndZipFiles.binaries
    echo returned an error. Please inspect the screen output for details.
    popd
    goto :eof
)

:: Safe the setups, then remove temporarily used files. Suppress related messages.
cd /D "%TEMP%"
move "%tmpCheckoutFolder%\components\linNet\cm\createSetup\%setupFileNameWithSrcs%" .
move "%tmpCheckoutFolder%\components\linNet\cm\createSetup\%setupFileNameBinaries%" .
rmdir /S/Q "%tmpCheckoutFolder%" > nul 2> nul

:: Move target files from temporary directory to final location.
popd
move "%TEMP%\%setupFileNameWithSrcs%" .
move "%TEMP%\%setupFileNameBinaries%" .

echo *****************************************************************************************
echo Please, find the newly created archives %setupFileNameWithSrcs% and
echo %setupFileNameBinaries% in your local working directory:
dir *.zip
echo *****************************************************************************************

goto :eof

:LUsage
echo usage: createSetup [revision] svn-tag
echo   CAUTION: Run this script exclusively from the directory cm\createSetup in the
echo component directory!
echo   The script creates two archives containing the linNet installation (with and without
echo the source files). The bundled files are taken from the project's SVN repository and
echo are addressed by the name of the wanted version's tag and optionally by a revision
echo number of this tag. If revision is omitted the head version of the tag is used.
echo   The created archives linNet-*.zip are found in the current working directory.
echo svn-tag: Refers to a revision in the folder "tags" in the SVN repository. Only
echo versioned file sets can be exported.
echo   Alternatively, but for testing purpose only, revision may either be trunk or branches.
echo In either case the head revision is addressed. If trunk is stated, svn-tag becomes
echo useless and must not be given.