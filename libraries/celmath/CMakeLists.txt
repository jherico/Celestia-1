set(TARGET_NAME celmath)
file(GLOB_RECURSE COMMON_SOURCES *.c *.cpp *.h *.hpp)
add_library(${TARGET_NAME} STATIC ${COMMON_SOURCES})
set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "libraries")

target_include_directories(${TARGET_NAME} PRIVATE ..)
target_eigen()
