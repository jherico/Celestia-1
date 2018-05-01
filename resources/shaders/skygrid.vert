#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec4 inPos;

layout (binding = 0) uniform Camera {
    mat4 projection;
    mat4 view;
} camera;

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
	gl_Position = camera.projection * camera.view * inPos;
    //gl_Position = pushConsts.orientation * inPos;
}

