# Using CMake to build the code

CMake version 2.8 or newer is required.

Run CMake:

$ cmake .

Now run the make command to compile the code:

$ make

The result will be a static library called libzerocoin.a.

To make a debug version of the code, add -DCMAKE_BUILD_TYPE=DEBUG
to the cmake command line:

$ cmake -DCMAKE_BUILD_TYPE=DEBUG ..

When compiling the code with 'make' you can set VERBOSE=1 to show
the process in more detail:

$ make VERBOSE=1
