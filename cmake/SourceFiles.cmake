set(SOURCE_FILES
   src/aund.c
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
   src/user_null.c
)

if(ENABLE_BEEBEM)
   list(APPEND SOURCE_FILES src/beebem.c)
endif()

if(ENABLE_AUN)
   list(APPEND SOURCE_FILES src/aun.c)
endif()

if(ENABLE_FEMTO_ECONET)
   list(APPEND SOURCE_FILES src/femto_econet.c)
endif()

if(ENABLE_FULL_CFGFILE)
   list(APPEND SOURCE_FILES src/conf_lex.c)
endif()

if(ENABLE_USERS)
   list(APPEND SOURCE_FILES src/pw.c)
endif()

if(ENABLE_LOGGING)
   list(APPEND SOURCE_FILES src/log.c)
endif()

# This adds a path to the generated config.h
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(${PROJECT_NAME} ${SOURCE_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS_RELEASE -s)
