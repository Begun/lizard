SET (TARGET_NAME lz_test_static)
ADD_EXECUTABLE (${TARGET_NAME} ../test-module/test_plugin.cpp)
TARGET_LINK_LIBRARIES (${TARGET_NAME} lizard-common lizard-static)
INSTALL (TARGETS ${TARGET_NAME} DESTINATION bin)
