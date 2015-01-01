To build linNet cd here in a shell and type:

make help
make -s build

or

make -s build CONFIG=PRODUCTION

The GCC tool chain needs to be on the system search path. GNU make 3.81 or
higher is required.

The build has been tested with GCC under Windows, using MinGW gcc 4.5.2
and MinGW-W64 gcc 4.8.1 and under Lunix (Fedora 18 distribution) with gcc
4.7.2.

For more details on the build of linNet, and in particular for the build on
systems other than Windows, please refer to the manual
https://linnet.googlecode.com/svn/trunk/components/linNet/doc/userGuide/linNet-userGuide.pdf