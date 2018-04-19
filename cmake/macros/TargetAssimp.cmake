#
#  Created by Bradley Austin Davis on 2016/06/03
#  Copyright 2013-2016 High Fidelity, Inc.
#
#  Distributed under the Apache License, Version 2.0.
#  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
#
macro(TARGET_ASSIMP)
    if (ANDROID)
        set(ASSIMP_INCLUDE_DIRS "C:/Android/vulkan/assimp/include")
        set(ASSIMP_LIBRARIES "C:/Android/vulkan/assimp/lib/libassimp.so")
    elseif(WIN32)
        add_dependency_external_projects(assimp)
        add_dependencies(${TARGET_NAME} assimp)
    else()
        find_package(assimp)
        link_directories(${ASSIMP_LIBRARY_DIRS})
        set(ASSIMP_INCLUDE_DIRS ${ASSIMP_INCLUDEDIR})
    endif()

    target_include_directories(${TARGET_NAME} PUBLIC ${ASSIMP_INCLUDE_DIRS})
    target_link_libraries(${TARGET_NAME} ${ASSIMP_LIBRARIES})
endmacro()
