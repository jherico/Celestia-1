# 
#  Created by Bradley Austin Davis on 2016/02/16
#
#  Distributed under the Apache License, Version 2.0.
#  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
# 
macro(TARGET_VULKAN)
    if (ANDROID)
        set(Vulkan_INCLUDE_DIR $ENV{VULKAN_SDK}/include)
        set(Vulkan_LIBRARIES vulkan)
    else()
        find_package(Vulkan REQUIRED)
        set(Vulkan_LIBRARIES Vulkan::Vulkan)
    endif()    
    target_link_libraries(${TARGET_NAME} ${Vulkan_LIBRARIES})
    target_include_directories(${TARGET_NAME} PUBLIC ${Vulkan_INCLUDE_DIR})
endmacro()