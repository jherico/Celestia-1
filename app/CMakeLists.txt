set(TARGET_NAME celestia)
file(GLOB_RECURSE COMMON_SOURCES *.c *.cpp *.h *.hpp)
add_executable(${TARGET_NAME} ${COMMON_SOURCES})

target_eigen()
depend_libraries(celutil celastro celmath celephem celmodel celengine)
target_qt(Core Gui)