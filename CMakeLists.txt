project(Anoncoin)
cmake_minimum_required(VERSION 3.0)

set(MAJOR_VERSION 0)
set(MINOR_VERSION 9)
set(REVISION_VERSION 6)
set(BUILD_VERSION 13)
set(VERSION ${MAJOR_VERSION}.${MINOR_VERSION}.${REVISION_VERSION}.${BUILD_VERSION})

set(COPYRIGHT_YEAR "2017")


# secp256k1 requires c99 for in-for-loop variable declarations.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake/Modules)
INCLUDE(UsePkgConfig)

find_package(Threads REQUIRED) # alert.cpp requires pthreads directly?
find_package(BDB REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost 1.45.0 COMPONENTS system filesystem thread program_options REQUIRED)
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

# bitcoin-qt dependencies (4.4.3 arbitrarily set via docs):
find_package(Qt4 4.4.3 COMPONENTS QtCore QtGui QtNetwork QtDbus)
find_package(Protobuf)
find_package(QRencode)

if(QT_FOUND AND PROTOBUF_FOUND)
    set(HAVE_BITCOINQT_DEPS TRUE)
else()
    set(HAVE_BITCOINQT_DEPS FALSE)
endif()

# TODO: autotools added -DMINIUPNP_STATICLIB support.
find_package(Miniupnpc)

if(BUILD_QT AND QRENCODE_FOUND)
    set(QRENCODE_DEPS_FOUND TRUE)
else()
    set(QRENCODE_DEPS_FOUND FALSE)
endif()


# Check if struct sockaddr_in6 contains sin6
include(CheckStructHasMember)
CHECK_STRUCT_HAS_MEMBER("struct sockaddr_in6" sin6_addr netinet/in.h HAVE_IPV6)

# src/compat.h and src/compat/strnlen.cpp need HAVE_DECL_STRNLEN:
include(CheckFunctionExists)
CHECK_FUNCTION_EXISTS(strnlen HAVE_STRNLEN)
if(HAVE_STRNLEN)
    add_definitions(-DHAVE_DECL_STRNLEN=1)
else()
    add_definitions(-DHAVE_DECL_STRNLEN=0)
endif()



#-----------------------------------------------------------------------------
# Options for building

option(BUILD_VERBOSE       "Verbose build output." ON)
option(ENABLE_TESTS        "Compile and enable unit tests." FALSE)
option(ENABLE_WALLET       "Enable wallet feature,)" ON)
option(ENABLE_I2PSAM       "Enable I2P feature,)" ON)
option(GLIBC_BACK_COMPAT   "Enable backwards compatibility with glibc and libstdc++" OFF)
option(BUILD_QT            "Build anoncoin-qt GUI" ${HAVE_BITCOINQT_DEPS})
option(USE_UPNP            "Whether UPnP support should be available or not." ${MINIUPNP_FOUND})
option(USE_UPNP_DEFAULT    "UPnP is enabled on startup" ${MINIUPNP_FOUND})
option(USE_QRCODE          "Enable QR code support" ${QRENCODE_DEPS_FOUND})
option(USE_DBUS            "Enable DBus support" ${QT_QTDBUS_FOUND})
option(RPM                 "Build a RPM" OFF)
option(DEB                 "Build a DEB" OFF)
set(PACKAGE_CONTACT        "Mikal 'Meeh' Villa"        CACHE <TYPE>  "Package Maintainer")
set(PACKAGE_VENDOR         "Anoncoin Core Developers"  CACHE <TYPE>  "Package Vendor")




# ripped from lclc's secp256k1 CMakeLists:
# https://github.com/lclc/secp256k1-cmake
option(SECP_ENDOMORPHISM "Enable secp256k1 Endomorphism." OFF)

# hardcoding to failsafe gmp for now
set(SECP_FIELD "gmp" CACHE STRING "secp256k1 Field operations: gmp|64bit|64bit_asm|32bit")
set(SECP_BIGNUM "gmp" CACHE STRING "secp256k1 Bignum Implementation: gmp|openssl")
set(SECP_SCALAR "4x64" CACHE STRING "secp256k1 Scalar Implementation: 4x64|8x32")

# Make SECP options combobox when cmake-gui is available
set_property(CACHE SECP_FIELD PROPERTY STRINGS "gmp" "64bit" "64bit_asm" "32bit")
set_property(CACHE SECP_BIGNUM PROPERTY STRINGS "gmp" "openssl")
set_property(CACHE SECP_SCALAR PROPERTY STRINGS "4x64" "8x32")

if(BUILD_VERBOSE)
   set(CMAKE_VERBOSE_MAKEFILE true)
endif()

if(BUILD_QT AND NOT HAVE_BITCOINQT_DEPS)
    message(FATAL_ERROR "BUILD_QT was requested but there are unmet dependencies:")
    if(NOT QT_FOUND)
        message(FATAL_ERROR " * Qt libraries unsatisfied (QtCore QtGui QtNetwork).")
    endif()
    if(NOT PROTOBUF_FOUND)
        message(FATAL_ERROR " * Protobuf library not found.")
    endif()
endif()

if(BUILD_QT AND NOT ENABLE_WALLET)
    message(STATUS "BUILD_QT requires ENABLE_WALLET. Enabling wallet implicitly.")
    set(ENABLE_WALLET ON)
endif()

if(USE_UPNP AND NOT MINIUPNP_FOUND)
    message(FATAL_ERROR "UPNP Requested but miniupnpc library not found.")
endif()

if(USE_UPNP_DEFAULT AND NOT USE_UPNP)
    message(STATUS "Implicitly disabling USE_UPNP_DEFAULT due to USE_UPNP not being enabled.")
    set(USE_UPNP_DEFAULT OFF)
endif()
if(USE_UPNP_DEFAULT)
    set(UPNP_SETTING "1") # turn on UPNP by default on bitcoin startup
else()
    set(UPNP_SETTING "0")
endif()

if(ENABLE_IPV6 AND NOT HAVE_IPV6)
    message(FATAL_ERROR "USE_IPV6 requested but unable to find development support.")
endif()
if(ENABLE_IPV6)
    set(USE_IPV6 1)
else()
    set(USE_IPV6 0)
endif()

if(USE_QRCODE AND NOT QRENCODE_DEPS_FOUND)
    message(FATAL_ERROR "USE_QRCODE requested but unable to find qt or libqrencode dependencies.")
endif()

if(USE_DBUS)
    if(NOT BUILD_QT)
        message(FATAL_ERROR "USE_DBUS requires BUILD_QT to be set.")
    endif()
    if(NOT QT_QTDBUS_FOUND)
        message(FATAL_ERROR "USE_DBUS requested but QtDbus not found.")
    endif()
endif()

if(RPM OR DEB)
    if(NOT PACKAGE_CONTACT)
        message(FATAL_ERROR "Specify a PACKAGE_CONTACT to build packages.")
    endif()
endif()


#-----------------------------------------------------------------------------
# Print system information and build options

message(STATUS "General -------------------------------------")
message(STATUS "Anoncoin version:       ${VERSION}")
message(STATUS "CMake version:          ${CMAKE_VERSION}")
message(STATUS "System:                 ${CMAKE_SYSTEM}")
message(STATUS "Processor:              ${CMAKE_SYSTEM_PROCESSOR}")

message(STATUS "Verbose:                ${BUILD_VERBOSE}")
message(STATUS "Testing:                ${ENABLE_TESTS}")

message(STATUS "Packaging -----------------------------------")
message(STATUS "Build RPM:              ${RPM}")
message(STATUS "Build DEB:              ${DEB}")
message(STATUS "Package Contact:        ${PACKAGE_CONTACT}")
message(STATUS "Package Vendor:         ${PACKAGE_VENDOR}")

message(STATUS "Build Options -- ----------------------------")
message(STATUS "Wallet feature:         ${ENABLE_WALLET}")
message(STATUS "bitcoin-qt GUI:         ${BUILD_QT}")
message(STATUS "UPnP:                   ${USE_UPNP}")
message(STATUS "UPnP on startup:        ${USE_UPNP_DEFAULT}")
message(STATUS "IPv6:                   ${ENABLE_IPV6}")
message(STATUS "QR code:                ${USE_QRCODE}")
message(STATUS "DBUS:                   ${USE_DBUS}")

message(STATUS "SECP256k1 Options ---------------------------")
message(STATUS "Endomorphism: ${SECP_ENDOMORPHISM}")
message(STATUS "Field operations: ${SECP_FIELD}")
message(STATUS "Bignum Implementation: ${SECP_BIGNUM}")
message(STATUS "Scalar Implementation: ${SECP_SCALAR}")

message(STATUS "Backwards compatibility with glibc and libstdc++: ${GLIBC_BACK_COMPAT}")


#-----------------------------------------------------------------------------
# Source Definitions

add_definitions(-DCLIENT_VERSION_MAJOR=${MAJOR_VERSION})
add_definitions(-DCLIENT_VERSION_MINOR=${MINOR_VERSION})
add_definitions(-DCLIENT_VERSION_REVISION=${REVISION_VERSION})
add_definitions(-DCLIENT_VERSION_BUILD=${BUILD_VERSION})
add_definitions(-DCOPYRIGHT_YEAR="${COPYRIGHT_YEAR}")

set(PACKAGE bitcoin)
set(host ${CMAKE_SYSTEM} ${CMAKE_SYSTEM_PROCESSOR})
set(COPYRIGHT "Copyright (c) 2013-${COPYRIGHT_YEAR} Anoncoin Developers.  All Rights Reserved.")
set(prefix ${CMAKE_INSTALL_PREFIX})

set(CLIENT_VERSION_IS_RELEASE OFF CACHE BOOL "Set ON to consider this build to be of release quality.  This disables warning messages that say this build is still in development.")
if(CLIENT_VERSION_IS_RELEASE)
    add_definitions(-DCLIENT_VERSION_IS_RELEASE=1)
else()
    add_definitions(-DCLIENT_VERSION_IS_RELEASE=0)
endif()

if(ENABLE_WALLET)
    add_definitions(-DENABLE_WALLET)
endif()

if(MSG_NOSIGNAL)
    add_definitions(-DMSG_NOSIGNAL)
endif()

if(HAVE_MSG_NOSIGNAL)
    add_definitions(-DHAVE_MSG_NOSIGNAL)
endif()

if(USE_UPNP_DEFAULT)
    add_definitions(-DUSE_UPNP)
endif()

if(ENABLE_IPV6)
    add_definitions(-DUSE_IPV6)
endif()

if(USE_QRCODE)
    add_definitions(-DUSE_QRCODE)
endif()

if(USE_DBUS)
    add_definitions(-DUSE_DBUS)
endif()

if(ENABLE_I2PSAM)
  add_definitions(-DENABLE_I2PSAM)
endif()

#-----------------------------------------------------------------------------
# secp256k1 preprocessor definitions.  Taken from lclc's CMakeLists.txt:
# https://github.com/lclc/secp256k1-cmake

if(SECP_ENDOMORPHISM)
    add_definitions(-DUSE_ENDOMORPHISM)
endif()

if(SECP_FIELD MATCHES 64bit_asm)
    # deleted: src/secp256k1/src/field_5x52_asm.asm
    message(FATAL_ERROR "64bit_asm field support appears to be removed from upstream.")

    if(NOT "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "x86_64")
        message(FATAL_ERROR "64bit_asm field support explicitly requested but is not compatible with this host.")
    else()
        enable_language(ASM_NASM)
        if("ASM${ASM_DIALECT}" STREQUAL "YASM")
            add_definitions(-DUSE_FIELD_5X52)
            add_definitions(-DUSE_FIELD_5X52_ASM)
            set(USE_ASM 1)
        else()
            message(FATAL_ERROR "64bit_asm field support explicitly requested but yasm was not found.")
        endif()
    endif()
elseif(SECP_FIELD MATCHES gmp) #default
    set(USE_GMP 1)
    add_definitions(-DUSE_FIELD_GMP)
    add_definitions(-DHAVE_LIBGMP)
elseif(SECP_FIELD MATCHES 64bit)
    add_definitions(-DUSE_FIELD_5X52)
    add_definitions(-DUSE_FIELD_5X52_INT128)
elseif(SECP_FIELD MATCHES 32bit)
    add_definitions(-DUSE_FIELD_10X26)
else()
    message(FATAL_ERROR "Invalid SECP Field implementation.")
endif()

if(SECP_BIGNUM MATCHES gmp) #default
    set(USE_GMP 1)
    add_definitions(-DHAVE_LIBGMP)
    add_definitions(-DUSE_NUM_GMP)
    add_definitions(-DUSE_FIELD_INV_NUM)
    add_definitions(-DUSE_SCALAR_INV_NUM)
elseif(SECP_BIGNUM MATCHES openssl)
    #find_package(OpenSSL REQUIRED) # OpenSSL always required in bitcoin core
    add_definitions(-DUSE_NUM_OPENSSL)
    add_definitions(-DUSE_FIELD_INV_BUILTIN)
    add_definitions(-DUSE_SCALAR_INV_BUILTIN)
else()
    message(FATAL_ERROR "Invalid SECP Bignum implementation.")
endif()

if(SECP_SCALAR MATCHES 8x32)
    add_definitions(-DUSE_SCALAR_8X32)
else()
    add_definitions(-DUSE_SCALAR_4X64) # arbitrary default
endif()

if(USE_GMP)
    find_path(GMP_INCLUDE_DIR gmp.h)
    if(GMP_INCLUDE_DIR STREQUAL "GMP_INCLUDE_DIR-NOTFOUND")
        message(FATAL_ERROR "Couldn't find gmp.h. Please install GMP.")
    else()
        message(STATUS "Found GMP: ${GMP_INCLUDE_DIR}")
    endif()
endif()



# This is probably a hack to get PRId64 defines from <inttypes.h> which
# doesn't seem to define them without manual intervention.
add_definitions(-D__STDC_FORMAT_MACROS)

# Remove -DNDEBUG from release builds because src/main.cpp:30
# requires assertions to always be enabled.
string(REGEX REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_MINSIZEREL ${CMAKE_CXX_FLAGS_MINSIZEREL})
string(REGEX REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
string(REGEX REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO})
string(REGEX REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS_MINSIZEREL ${CMAKE_C_FLAGS_MINSIZEREL})
string(REGEX REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE})
string(REGEX REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS_RELWITHDEBINFO ${CMAKE_C_FLAGS_RELWITHDEBINFO})


#-----------------------------------------------------------------------------
# Collect all binaries into subdirectory

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  add_definitions(-DOS_MACOSX)
endif()

#-----------------------------------------------------------------------------
# compiler options

# missing AX_CHECK_LINK_FLAG() from autoconf.. so workaround:
include(CheckCCompilerFlag)


if(MINGW)
    # Allow windows builds to handle addresses larger than 2GB.
    set(CMAKE_REQUIRED_FLAGS "-Wl,--large-address-aware")
    CHECK_C_COMPILER_FLAG("" LINK_LARGE_ADDRESS_AWARE)
    if(LINK_LARGE_ADDRESS_AWARE)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--large-address-aware")
    endif()
endif()


if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    # This flag screws up non-darwin gcc even when the check fails. special-case it.
    set(CMAKE_REQUIRED_FLAGS "-Wl,-dead_strip")
    CHECK_C_COMPILER_FLAG("" LINK_DEAD_STRIP)
    if(LINK_DEAD_STRIP)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-dead_strip")
    endif()
endif()

if(UNIX)
    option(ENABLE_HARDENING "Attempt to harden the resulting executables (default is yes)" ON)
    set(HARDENED_CXXFLAGS "-Wstack-protector -fPIE" CACHE STRING "")
    set(HARDENED_CPPFLAGS "-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2" CACHE STRING "")
    if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
      set(HARDENED_LDFLAGS "-Wl" CACHE STRING "")
      #set(HARDENED_LDFLAGS "-Wl,-z,relro" CACHE STRING "")
      #set(HARDENED_LDFLAGS "${HARDENED_LDFLAGS} -Wl,-z,now" CACHE STRING "")
    else()
      set(HARDENED_LDFLAGS "-Wl" CACHE STRING "")
    endif()

    set(CMAKE_REQUIRED_FLAGS "-Wl,--dynamicbase")
    CHECK_C_COMPILER_FLAG("" LINK_DYNAMIC_BASE)
    if(LINK_DYNAMIC_BASE)
        set(HARDENED_LDFLAGS "${HARDENED_LDFLAGS} -Wl,--dynamicbase" CACHE STRING "")
    endif()

    set(CMAKE_REQUIRED_FLAGS "-Wl,--nxcompat")
    CHECK_C_COMPILER_FLAG("" LINK_NX_COMPAT)
    if(LINK_NX_COMPAT)
        set(HARDENED_LDFLAGS "${HARDENED_LDFLAGS} -Wl,--nxcompat" CACHE STRING "")
    endif()

    if(NOT MINGW)
        # -fstack-protector-all can produce broken binaries with mingw
  set(HARDENED_CXXFLAGS "${HARDENED_CXXFLAGS} -fno-stack-protector" CACHE STRING "")
  set(HARDENED_CXXFLAGS "${HARDENED_CXXFLAGS} -fstack-protector-all" CACHE STRING "")
        #  -pie will link successfully with MinGW, but it's unsupported
  #  and leads to undeterministic binaries
  set(HARDENED_LDFLAGS "${HARDENED_LDFLAGS} -pie" CACHE STRING "")
    endif()
    MARK_AS_ADVANCED(HARDENED_CXXFLAGS HARDENED_CPPFLAGS HARDENED_LDFLAGS)

    if(ENABLE_HARDENING)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${HARDENED_CXXFLAGS} ${HARDENED_CPPFLAGS}")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${HARDENED_LDFLAGS}")
    endif()
endif()


#-----------------------------------------------------------------------------
# Build Unit tests

if(ENABLE_TESTS)
    enable_testing()
    find_package(Boost 1.45.0 COMPONENTS unit_test_framework)
    message(STATUS "Tests temporarily disabled to avoid conversion of .json data files.")
    SET(ENABLE_TESTS FALSE)
endif()


#-----------------------------------------------------------------------------
# Build source


add_subdirectory(src)


#-----------------------------------------------------------------------------
# Packaging
set(CPACK_GENERATOR "TGZ")

set(CPACK_PACKAGE_VERSION ${VERSION})
set(CPACK_PACKAGE_NAME "Anoncoin Core")
set(CPACK_PACKAGE_CONTACT ${PACKAGE_CONTACT})
set(CPACK_PACKAGE_VENDOR ${PACKAGE_VENDOR})
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}.${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Anoncoin is an experimental new digital currency that enables instant payments to anyone, anywhere in the world.")
set(CPACK_PACKAGE_DESCRIPTION "
Anoncoin is an experimental new digital currency that enables instant payments to anyone, anywhere in the world. Anoncoin uses peer-to-peer technology to operate with no central authority: managing transactions and issuing money are carried out collectively by the network.
")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
set(WEBSITE "https://www.anoncoin.net/")

if(RPM)
  set(CPACK_GENERATOR "RPM")
  set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
#  set(CPACK_RPM_CHANGELOG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/ChangeLog")
endif(RPM)

if(DEB)
  set(CPACK_GENERATOR "DEB")
  #set(CPACK_DEBIAN_PACKAGE_DEPENDS "")
  set(CPACK_DEBIAN_PACKAGE_PRIORITY "extra")
  set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
  set(CPACK_DEBIAN_PACKAGE_HOMEPAGE ${WEBSITE})
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER ${PACKAGE_CONTACT})
endif(DEB)

include(CPack)
