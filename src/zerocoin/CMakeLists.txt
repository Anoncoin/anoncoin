# Copyright 2013 Corgan Labs
# This file is part of the Zerocoin project
# See LICENSE file or http://opensource.org/licenses/MIT for terms


if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Release")
   message(STATUS "Build type not specified: defaulting to release.")
endif(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "")

#
# Setup overall project
#
cmake_minimum_required(VERSION 2.8)
project(zerocoin)

# Appended to CMAKE_INSTALL_PREFIX
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/CMake/Modules)

#
# Project-wide package dependencies
#
# Find and configure the Boost C++ libraries
include(BoostConfig)

# Find and configure OpenSSL crypto library
include(FindOpenSSL)

#
# Project-wide directory settings
#

#
# Project-wide installation settings
#
set(ZEROCOIN_INCLUDE_DIR ".")
set(ZEROCOIN_BIN_DIR     bin)
set(ZEROCOIN_LIB_DIR     lib)

# Disable I2P for the libzerocoin library
add_definitions(-DZC_DISABLE_I2P)


#
# pkg-config substitution variables
#
file(TO_NATIVE_PATH "${CMAKE_INSTALL_PREFIX}"             prefix)
file(TO_NATIVE_PATH "\${prefix}"                          exec_prefix)
file(TO_NATIVE_PATH "\${exec_prefix}/${ZEROCOIN_LIB_DIR}" libdir)
file(TO_NATIVE_PATH "\${prefix}/${ZEROCOIN_INCLUDE_DIR}"  includedir)

#
# Add individual directories to project
#
#add_subdirectory(foo)

#
# Create uninstall target
#
configure_file(
  ${CMAKE_SOURCE_DIR}/CMake/cmake_uninstall.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
@ONLY)

add_custom_target(uninstall
  ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
)

########################################################################
# Shared library generation libzerocoin.so
########################################################################

#
# Tell compiler where to search for include files
#
include_directories(
  ${Boost_INCLUDE_DIRS}
  ${OPENSSL_INCLUDE_DIR}
)

#
# Tell linker where to look for library files
#
link_directories(
  ${Boost_LIBRARY_DIRS}
)

#
# List of libraries to link in
#
list(APPEND zerocoin_libs
  ${Boost_LIBRARIES}
  ${OPENSSL_CRYPTO_LIBRARY}
)

#
# List of source files to compile
#
list(APPEND zerocoin_sources
  Accumulator.cpp
  AccumulatorProofOfKnowledge.cpp
  Coin.cpp
  CoinSpend.cpp
  Commitment.cpp
  ParamGeneration.cpp
  Params.cpp
  SerialNumberSignatureOfKnowledge.cpp
  SpendMetaData.cpp
)

#
# Create static library libzerocoin.so
#
add_library(zerocoin STATIC ${zerocoin_sources})
target_link_libraries(zerocoin ${zerocoin_libs})

#
# Install static library
#
install(TARGETS zerocoin
	ARCHIVE DESTINATION ${ZEROCOIN_LIB_DIR}
)

#
# Install header files
#
install(FILES
  ../allocators.h
  ../bignum.h
  ../clientversion.h
  ../compat.h
  ../hash.h
  ../netbase.h
  ../serialize.h
  ../uint256.h
  ../version.h
  DESTINATION ${ZEROCOIN_INCLUDE_DIR}/../
)

install(FILES
  Accumulator.h
  AccumulatorProofOfKnowledge.h
  Coin.h
  CoinSpend.h
  Commitment.h
  ParamGeneration.h
  Params.h
  SerialNumberSignatureOfKnowledge.h
  SpendMetaData.h
  SetupOpenSSL.h
  ../Zerocoin.h
  DESTINATION ${ZEROCOIN_INCLUDE_DIR}
)

# GNOSIS 2014-09-03: no need for pkgconfig files if only used by Anoncoin
##
## Create pkg-config file
##
#configure_file(
#  ${CMAKE_CURRENT_SOURCE_DIR}/zerocoin.pc.in
#  ${CMAKE_CURRENT_BINARY_DIR}/zerocoin.pc
#@ONLY)
#
##
## Install pkg-config file
##
#install(
#  FILES ${CMAKE_CURRENT_BINARY_DIR}/zerocoin.pc
#  DESTINATION ${ZEROCOIN_LIB_DIR}/pkgconfig
#)

########################################################################
# Executable files
########################################################################

find_package(Threads)

#add_executable(paramgen ../util.cpp paramgen.cpp)
#target_link_libraries(paramgen zerocoin)

add_executable(tutorial Tutorial.cpp)
target_link_libraries(tutorial zerocoin ${CMAKE_THREAD_LIBS_INIT})

add_executable(ufo PrintUFOCandidate.cpp)
target_link_libraries(ufo zerocoin ${CMAKE_THREAD_LIBS_INIT})
