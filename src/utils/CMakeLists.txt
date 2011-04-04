ADD_LIBRARY (lz_utils STATIC daemon.cpp logger.cpp parxml.cpp)
TARGET_LINK_LIBRARIES (lz_utils ${Boost_THREAD_LIBRARY})
INSTALL (TARGETS ${TARGET_NAME} DESTINATION lib)
