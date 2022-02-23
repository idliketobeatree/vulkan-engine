#version 450

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 color;

layout(set = 0, binding = 0) uniform CameraData {
    vec3 position;
} cameraData;

void main() {
    color = vec4(fragColor, 1.0);
//    color = vec4(cameraData.position, 1.0);
}
