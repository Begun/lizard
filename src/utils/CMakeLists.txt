ADD_LIBRARY (lz_utils STATIC daemon.cpp logger.cpp parxml.cpp)
INSTALL (TARGETS ${TARGET_NAME} DESTINATION lib)
