cmake_minimum_required(VERSION 2.8)
project(matrix)

add_subdirectory(src)
add_subdirectory(contrib)
add_subdirectory(slogger)
add_subdirectory(keychain)
add_subdirectory(unit_tests)
add_subdirectory(examples/ToyScope)
add_subdirectory(examples/Helloworld)

# To install in a non-standard (e.g. not /usr/lib)
# specify the installation directory on the command line
# when configuring. The make install will then install into
# that directory. Note this is similar to the --prefix= of
# autotools, so the actual directory will be under lib, bin, 
# include etc.
#
# THIRDPARTYDIR must be specified in all cases. What the
# THIRDPARYTDIR is specifying is the location of the dependencies
# e.g yaml-cpp and zmq, not the location of matrix include files.
#
# Typical configuration command:
# cmake .. -DCMAKE_INSTALL_PREFIX=/home/gbt1/lib_16.3 -DTHIRDPARTYDIR=/home/gbt1/lib_16.3
