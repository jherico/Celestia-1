#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

#include "camera.h"

layout (location = 0) in vec4 inPosAndSize;
layout (location = 1) in vec4 inColor;

layout (location = 0) out vec4 outColor;

out gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
};

void main() {
    outColor = inColor;
    gl_PointSize = inPosAndSize.w;
    gl_Position = camera.projection * camera.view * vec4(inPosAndSize.xyz, 1.0);
}

