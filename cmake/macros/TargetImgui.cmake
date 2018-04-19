macro(TARGET_IMGUI)
    set(IMGUI_DIR "${PROJECT_SOURCE_DIR}/external/imgui")
    file(GLOB IMGUI_SOURCES 
        ${IMGUI_DIR}/*.c
        ${IMGUI_DIR}/*.cpp
        ${IMGUI_DIR}/*.h
        ${IMGUI_DIR}/*.hpp
    )
    add_library(imgui STATIC ${IMGUI_SOURCES})
    set_target_properties(imgui PROPERTIES FOLDER "externals")
    add_dependencies(${TARGET_NAME} imgui)
    target_include_directories(${TARGET_NAME} PUBLIC ${IMGUI_DIR})
endmacro()