#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (set = 1, binding = 0) uniform sampler2D samplerColor;

layout (location = 0) in vec4 inColor;
layout (location = 0) out vec4 outFragColor;

void main() {
	outFragColor = texture(samplerColor, gl_PointCoord).r * inColor;
}