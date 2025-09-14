set(SOURCE_FILES
   aun.c
   aund.c
   beebem.c
   conf_lex.c
   fileserver.c
   fs_cli.c
   fs_error.c
   fs_examine.c
   fs_fileio.c
   fs_filetype.c
   fs_handle.c
   fs_misc.c
   fs_nametrans.c
   fs_util.c
   pw.c
   user_null.c
)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

