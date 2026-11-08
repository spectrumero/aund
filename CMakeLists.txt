cmake_minimum_required(VERSION 3.18.1)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
   message(FATAL_ERROR "Prevented in-tree build. Please create a build directory, e.g. use cmake -B build")
endif ()

if(NOT DEFINED AUND_BUILD)
   set(AUND_BUILD standard)
endif ()

option(ENABLE_BEEBEM "Enable BeebEm style networking" ON)
option(ENABLE_AUN "Enable AUN over TCP/IP" ON)
option(ENABLE_FEMTO_ECONET "Enable native econet on Femto board" OFF)
option(ENABLE_FULL_CFGFILE "Enable full configuration file" ON)
option(ENABLE_USERS "Enable user file" ON)
option(ENABLE_LOGGING "Enable logging output" ON)
option(USE_CRYPT "Use crypt for pwfile" ON)

if(${AUND_BUILD} STREQUAL "standard")
   message(STATUS "Building standard aund")
   set(AUND_LANGUAGES C CXX)

elseif (${AUND_BUILD} STREQUAL "femto")
   message(STATUS "Building Femto aund")
   set(FEMTO 1)
   set(ENABLE_BEEBEM OFF)
   set(ENABLE_AUN OFF)
   set(ENABLE_FEMTO_ECONET ON)
   set(ENABLE_USERS OFF)
   set(USE_CRYPT OFF)
   set(ENABLE_FULL_CFGFILE OFF)
   set(ENABLE_LOGGING OFF)

   # Cross compile for the FileStick (must be set before project())
   set(CMAKE_SYSTEM_NAME Generic)
   set(CMAKE_SYSTEM_PROCESSOR riscv32)
   set(CMAKE_C_COMPILER riscv-none-elf-gcc)
   set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
   set(AUND_LANGUAGES C)

   # aund lives in a submodule of the FileStick tree by default
   set(FILESTICK_DIR "${CMAKE_SOURCE_DIR}/.." CACHE PATH "FileStick source tree")
   set(FILESTICK_LIB_DIR "${FILESTICK_DIR}/build/lib" CACHE PATH
      "Directory containing the built libfilestick.a")

else ()
   message(FATAL_ERROR "Unknown aund build type")
endif ()

set(VERSION_MAJOR "1")
set(VERSION_MINOR "08")
set(VERSION_PATCH "1")

set(VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

project(aund
   VERSION ${VERSION}
   LANGUAGES ${AUND_LANGUAGES}
   DESCRIPTION "Acorn AUN fileserver"
)

include(cmake/SourceFiles.cmake)
include(cmake/Dependencies.cmake)
if(FEMTO)
   include(cmake/Femto.cmake)
else()
   include(cmake/Install.cmake)
endif()

configure_file(src/config.h.in config.h)

