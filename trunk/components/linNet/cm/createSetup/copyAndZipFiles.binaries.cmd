@echo off
:: Compose file tree, call 7z and create the installation package, which contains only the
:: executable binary files.
::   Run this batch from the directory cm/createSetup only!

if "%1" == "" goto LUsage
if "%1" == "-h" goto LUsage
if "%1" == "-?" goto LUsage
if "%1" == "/?" goto LUsage
if "%1" == "/h" goto LUsage

setlocal

set tmpFolder=tmp.%1
rmdir /S/Q %tmpFolder% > nul 2> nul

:: Create the directory tree as aimed on the destination system.

mkdir %tmpFolder%
mkdir %tmpFolder%\linNet

:: Go back to the root of the project.
pushd ..\..\..\..
set output=components\linNet\cm\createSetup\%tmpFolder%\linNet

:: Global documentation
copy gpl.txt %output%
:: Goto the root of component linNet
popd
pushd ..\..
set output=cm\createSetup\%tmpFolder%\linNet
mkdir %output%\doc
:: The version file needs to be supplied by the caller of this script.
copy cm\createSetup\version.txt %output%
copy doc\installation.txt %output%\doc
copy doc\readMe.forSetupBinaries.txt %output%\readMe.txt

:: User guide without sources
mkdir %output%\doc\userGuide
copy doc\userGuide\linNet-userGuide.pdf %output%\doc\userGuide

:: The binaries
mkdir %output%\bin
xcopy /S bin\*.exe %output%\bin
mkdir %output%\bin\win32\DEBUG\private
xcopy /S Octave\private\* %output%\bin\win32\DEBUG\private
mkdir %output%\bin\win32\PRODUCTION\private
xcopy /S Octave\private\* %output%\bin\win32\PRODUCTION\private
mkdir %output%\bin\win64\DEBUG\private
xcopy /S Octave\private\* %output%\bin\win64\DEBUG\private
mkdir %output%\bin\win64\PRODUCTION\private
xcopy /S Octave\private\* %output%\bin\win64\PRODUCTION\private
mkdir %output%\bin\LINUX
mkdir %output%\bin\LINUX\DEBUG
mkdir %output%\bin\LINUX\DEBUG\private
copy bin\LINUX\DEBUG\linNet %output%\bin\LINUX\DEBUG
xcopy /S Octave\private\* %output%\bin\LINUX\DEBUG\private
mkdir %output%\bin\LINUX\PRODUCTION
mkdir %output%\bin\LINUX\PRODUCTION\private
copy bin\LINUX\PRODUCTION\linNet %output%\bin\LINUX\PRODUCTION
xcopy /S Octave\private\* %output%\bin\LINUX\PRODUCTION\private
attrib +R %output%\bin\* /S
attrib -R %output%\bin\*.exe /S
attrib -R %output%\bin\LINUX\DEBUG\linNet
attrib -R %output%\bin\LINUX\PRODUCTION\linNet

:: Copy some selected circuit files.
mkdir %output%\circuits
copy test\failingTestCases\bandStop.makesOctave3-4-3-win32Crash.cnl %output%\circuits
copy test\testCases\octagon.cnl %output%\circuits
copy test\testCases\octagon.jpg %output%\circuits
copy test\testCases\inverseTF.cnl %output%\circuits
copy test\testCases\largeAndSimple.cnl %output%\circuits
copy test\testCases\deviceAtSingleNode.cnl %output%\circuits
copy test\testCases\recursiveControlledVoltageSrcs.jpg %output%\circuits
copy test\testCases\recursiveControlledVoltageSrcs.cnl %output%\circuits
copy test\testCases\2poleLP.cnl %output%\circuits
copy test\testCases\3poleLP.cnl %output%\circuits
copy test\testCases\trickyOP.ckt %output%\circuits
copy test\testCases\trickyOP.jpg %output%\circuits
copy test\testCases\wire*.cnl %output%\circuits
copy test\testCases\wire*.log %output%\circuits
copy test\testCases\exampleManual.cnl %output%\circuits
copy test\testCases\NgtD.cnl %output%\circuits
copy test\testCases\rr.cnl %output%\circuits

:: After completing the aimed target file tree restore the initial working directory.
popd

:: Replace the current archive with the zipped temporary directory tree.
del /F/Q %1 > nul 2> nul
7za a -tzip %1 .\%tmpFolder%\* -r
if ERRORLEVEL 1 (
    echo Error: Files could not be archived completely
    exit /B 1
)

:: Remove the temporary directory tree again.
rmdir /S/Q %tmpFolder%

goto :eof

:LUsage
echo usage: copyAndZipFiles.binaries archive
echo archive designates the name of the aimed archive file to hold the binaries.
goto :eof
