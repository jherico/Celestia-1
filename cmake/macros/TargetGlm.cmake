macro(TARGET_GLM)
    add_dependency_external_projects(glm)
    add_dependencies(${TARGET_NAME} glm)
    target_include_directories(${TARGET_NAME} PUBLIC ${GLM_INCLUDE_DIRS})
endmacro()

