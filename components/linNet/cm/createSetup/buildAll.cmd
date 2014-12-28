@echo off
:: See usage message for help. See
:: http://en.wikibooks.org/wiki/Windows_Programming/Programming_CMD for syntax of batch
:: scripts.
goto LStart
:LUsage
echo usage: buildALl
echo   Build the executable files as far as possible (i.e. for the system this script is
echo running on). These files are part of the setup but they are not under source control.
echo This script is part of the setup generation process. The start directory is
echo cm\createSetup.
goto :eof

:: 
:: Copyright (c) 2014, Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
:: 

:LStart
::if /i "%1" == "" goto LUsage
if /i "%1" == "-h" goto LUsage
if /i "%1" == "-?" goto LUsage
if /i "%1" == "/?" goto LUsage
if /i "%1" == "/h" goto LUsage
:: Limit the allowed number of parameters.
if not "%1" == "" goto LUsage

setlocal

pushd "%tmpCheckoutFolder%\components\linNet"
echo Compiling the software in all configurations
make -s CONFIG=DEBUG rebuild
make -s CONFIG=PRODUCTION rebuild
make -s MINGW_HOME=C:\ProgramFiles\mingw64 CONFIG=DEBUG rebuild     
make -s MINGW_HOME=C:\ProgramFiles\mingw64 CONFIG=PRODUCTION rebuild
popd




