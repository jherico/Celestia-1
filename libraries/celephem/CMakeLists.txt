set(TARGET_NAME celephem)
file(GLOB_RECURSE COMMON_SOURCES *.c *.cpp *.h *.hpp)
add_library(${TARGET_NAME} STATIC ${COMMON_SOURCES})
set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "libraries")

depend_libraries(celastro celmath)
target_eigen()
target_cspice()
