#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec4 inPosAndSize;
layout (location = 1) in vec4 inColor;

layout (binding = 0) uniform Camera {
    mat4 projection;
    mat4 view;
} camera;

layout (location = 0) out vec4 outColor;

out gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
};

void main() {
    outColor = vec4(inColor.rgb, 1.0);
    //gl_PointSize = inPosAndSize.w;
    gl_PointSize = inColor.a * 6.0;
    gl_Position = camera.projection * camera.view * vec4(inPosAndSize.xyz, 1.0);
}

