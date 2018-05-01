set(TARGET_NAME celestia)
file(GLOB_RECURSE COMMON_SOURCES *.c *.cpp *.h *.hpp)
list(APPEND COMMON_SOURCES resources/resources.qrc)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
add_executable(${TARGET_NAME} ${COMMON_SOURCES})

add_dependencies(${TARGET_NAME} shaders)

target_eigen()
depend_libraries(celutil celastro celmath celephem celmodel celengine celapp vks)
target_qt(Core Gui)

set(EXTRA_DEPLOY_OPTIONS "--qmldir  \"${CMAKE_CURRENT_SOURCE_DIR}/resources/qml\"")
package_libraries_for_deployment()
