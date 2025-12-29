cmake_minimum_required(VERSION 3.18.1)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
   message(FATAL_ERROR "Prevented in-tree build. Please create a build directory, e.g. use cmake -B build")
endif ()

if(NOT DEFINED AUND_BUILD)
   set(AUND_BUILD standard)
endif ()

option(ENABLE_BEEBEM "Enable BeebEm style networking" ON)
option(ENABLE_FULL_CFGFILE "Enable full configuration file" ON)
option(ENABLE_USERS "Enable user file" ON)
option(USE_CRYPT "Use crypt for pwfile" ON)

if(${AUND_BUILD} STREQUAL "standard")
   message(STATUS "Building standard aund")

elseif (${AUND_BUILD} STREQUAL "femto")
   message(STATUS "Building Femto aund")
   set(FEMTO 1)
   set(ENABLE_BEEBEM OFF)
   set(USE_CRYPT OFF)
   set(ENABLE_FULL_CFGFILE OFF)

else ()
   message(FATAL_ERROR "Unknown aund build type")
endif ()

set(VERSION_MAJOR "1")
set(VERSION_MINOR "08")
set(VERSION_PATCH "1")

set(VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

project(aund
   VERSION ${VERSION}
   LANGUAGES C CXX
   DESCRIPTION "Acorn AUN fileserver"
)

include(cmake/SourceFiles.cmake)
include(cmake/Dependencies.cmake)
include(cmake/Install.cmake)

configure_file(src/config.h.in config.h)

