set(TARGET_NAME celutil)
file(GLOB COMMON_SOURCES *.c *.cpp *.h *.hpp)
add_library(${TARGET_NAME} STATIC ${COMMON_SOURCES})
set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "libraries")
