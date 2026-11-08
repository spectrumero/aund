# Build settings for aund on the FileStick (AUND_BUILD=femto).
#
# The FileStick has ~70K of RAM for a user program's code, data, heap and
# stack, so everything is built for size and unused sections are discarded.

set(FEMTO_ARCH_FLAGS -march=rv32imc -mabi=ilp32)

target_compile_options(${PROJECT_NAME} PRIVATE
   ${FEMTO_ARCH_FLAGS}
   -Os
   -ffunction-sections
   -fdata-sections
   -Wall
   -Wno-pointer-sign
)

# newlib only defines some errno values aund uses (EUSERS) with this
target_compile_definitions(${PROJECT_NAME} PRIVATE __LINUX_ERRNO_EXTENSIONS__)

# Shim headers for what newlib doesn't have (fts.h, err.h, dirent.h...)
# come first, then the FileStick headers (sys/econet.h, poll.h...).
target_include_directories(${PROJECT_NAME} BEFORE PRIVATE
   ${CMAKE_SOURCE_DIR}/src/femto/include
   ${FILESTICK_DIR}/include
)

# Every aund source gets femto_compat.h, which redirects the POSIX calls
# that need FileStick specific behaviour to src/femto/compat.c. The compat
# sources themselves call the real functions.
get_target_property(AUND_SOURCES ${PROJECT_NAME} SOURCES)
foreach(src ${AUND_SOURCES})
   if(src IN_LIST FEMTO_COMPAT_FILES)
      set_property(SOURCE ${src} APPEND PROPERTY COMPILE_DEFINITIONS
         FEMTO_COMPAT_IMPL)
   else()
      set_property(SOURCE ${src} APPEND PROPERTY COMPILE_OPTIONS
         -include ${CMAKE_SOURCE_DIR}/src/femto/include/femto_compat.h)
   endif()
endforeach()

set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".elf")

target_link_options(${PROJECT_NAME} PRIVATE
   ${FEMTO_ARCH_FLAGS}
   -L${FILESTICK_LIB_DIR}
   -specs=${FILESTICK_DIR}/lib/filestick.specs
   -Wl,--gc-sections
   -Wl,-Map=${PROJECT_NAME}.map
)
