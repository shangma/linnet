@echo off
:: Compose file tree, call 7z and create the installation package.
:: Run this batch from the directory cm/createSetup only!

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
mkdir %tmpFolder%\linNet\components
mkdir %tmpFolder%\linNet\components\linNet

:: Go back to the root of the project.
pushd ..\..\..\..
set output=components\linNet\cm\createSetup\%tmpFolder%\linNet\components\linNet

:: Global documentation. The version file needs to be supplied by the caller of this
:: script.
copy gpl.txt %output%
:: Goto the root of component linNet
popd
pushd ..\..
set output=cm\createSetup\%tmpFolder%\linNet\components\linNet
mkdir %output%\doc
copy cm\createSetup\version.txt %output%
copy doc\installation.txt %output%\doc
copy doc\parser-cnl-bnf.txt %output%\doc
copy doc\parser-ckt-bnf.txt %output%\doc
copy doc\readMe.forSetup.txt %output%\readMe.txt
copy doc\linnet.jpg %output%\doc

:: Doxygen documentation
copy doc\linnet_small.jpg %output%\doc
mkdir %output%\doc\doxygen
xcopy /S doc\doxygen\* %output%\doc\doxygen

:: User guide with sources
mkdir %output%\doc\userGuide
copy doc\userGuide\GNUmakefile %output%\doc\userGuide
copy doc\userGuide\linNet-userGuide.pdf %output%\doc\userGuide
copy doc\userGuide\readMe.txt %output%\doc\userGuide
mkdir %output%\doc\userGuide\sources
xcopy /S doc\userGuide\sources\* %output%\doc\userGuide\sources

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
copy test\testCases\testCases.vsd %output%\circuits
copy test\testCases\exampleManual.cnl %output%\circuits
copy test\testCases\NgtD.cnl %output%\circuits
copy test\testCases\rr.cnl %output%\circuits

:: The source code
copy doc\linnet.ico %output%\doc
xcopy doc\iconForCnl.* %output%\doc
mkdir %output%\code
xcopy /S code\* %output%\code

:: The makefiles
copy GNUmakefile %output%
:: Goto the root of component shared.
popd
pushd ..\..\..\shared
set output=..\linNet\cm\createSetup\%tmpFolder%\linNet\components\shared
mkdir %output%
mkdir %output%\makefile
xcopy /S makefile\*.mk %output%\makefile

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
echo usage: copyAndZipFiles archive
echo archive designates the name of the aimed archive file to hold component
echo linNet with all source files.
goto :eof
