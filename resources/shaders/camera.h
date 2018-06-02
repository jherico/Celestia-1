#extension GL_OVR_multiview : enable

struct Camera {
    mat4 projection;
    mat4 view;
};

layout(set = 0, binding = 0) uniform CameraBuffer {
    Camera cameras[2];
};

mat4 getProjection() {
    return cameras[gl_ViewID_OVR].projection;
}

mat4 getView() {
    return cameras[gl_ViewID_OVR].view;
}