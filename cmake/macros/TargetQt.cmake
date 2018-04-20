
macro(TARGET_QT)
  set(${TARGET_NAME}_DEPENDENCY_QT_MODULES ${ARGN})
  # find these Qt modules and link them to our own target
  find_package(Qt5 COMPONENTS ${${TARGET_NAME}_DEPENDENCY_QT_MODULES} REQUIRED CMAKE_FIND_ROOT_PATH_BOTH)
  foreach(QT_MODULE ${${TARGET_NAME}_DEPENDENCY_QT_MODULES})
    target_link_libraries(${TARGET_NAME} Qt5::${QT_MODULE})
  endforeach()
endmacro()