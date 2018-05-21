#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

#include "camera.h"

layout (location = 0) in vec4 inPos;

layout(push_constant) uniform PushConsts {
    mat4 orientation;
    vec4 color;
} pushConsts;


layout (location = 0) out vec4 outColor;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    outColor = pushConsts.color;
    gl_Position = camera.projection * camera.view * pushConsts.orientation * inPos;
}

