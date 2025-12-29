set(SOURCE_FILES
   src/aun.c
   src/aund.c
   src/conf_lex.c
   src/fileserver.c
   src/fs_cli.c
   src/fs_error.c
   src/fs_examine.c
   src/fs_fileio.c
   src/fs_filetype.c
   src/fs_handle.c
   src/fs_misc.c
   src/fs_nametrans.c
   src/fs_util.c
   src/pw.c
   src/user_null.c
)

if(ENABLE_BEEBEM)
   list(APPEND SOURCE_FILES src/beebem.c)
endif()

# This adds a path to the generated config.h
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(${PROJECT_NAME} ${SOURCE_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS_RELEASE -s)
