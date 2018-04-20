macro(depend_libraries)
  set(${TARGET_NAME}_DEPENDENCY_QT_MODULES )
  # find these Qt modules and link them to our own target
  foreach(CELLIB ${ARGN})
    add_dependencies(${TARGET_NAME} ${CELLIB})
    target_link_libraries(${TARGET_NAME} ${CELLIB})
    target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/libraries/${CELLIB}/src)
  endforeach()
endmacro()
