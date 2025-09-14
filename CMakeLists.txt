cmake_minimum_required(VERSION 3.18.1)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
   message(FATAL_ERROR "Prevented in-tree build. Please create a build directory, e.g. use cmake -B build")
endif ()

option(USE_CRYPT "Use crypt for pwfile" ON)

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

configure_file(config.h.in config.h)

