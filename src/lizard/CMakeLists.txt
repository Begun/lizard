SET (TARGET_NAME lizard-common)

SET (SRC
    fd_map.cpp
    http.cpp
    main.cpp
    server.cpp
    statistics.cpp
    utils.cpp
)

ADD_LIBRARY (${TARGET_NAME} STATIC ${SRC})
TARGET_LINK_LIBRARIES (${TARGET_NAME} lz_utils ${LJUDY} ${LEXPAT} pthread)
INSTALL (TARGETS ${TARGET_NAME} DESTINATION lib)

INSTALL (FILES ${PROJECT_SOURCE_DIR}/include/lizard/plugin.hpp DESTINATION include/lizard COMPONENT ${PROJECT_NAME})
