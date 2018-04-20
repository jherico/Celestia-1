set(TARGET_NAME celengine)
file(GLOB_RECURSE COMMON_SOURCES *.c *.cpp *.h *.hpp)
add_library(${TARGET_NAME} STATIC ${COMMON_SOURCES})
set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "libraries")

depend_libraries(celutil celastro celmath celephem celmodel)
target_eigen()
