if (USE_CRYPT)
   set (CRYPT_LIBRARIES crypt)
   target_link_libraries(${PROJECT_NAME} PRIVATE ${CRYPT_LIBRARIES})
endif ()

