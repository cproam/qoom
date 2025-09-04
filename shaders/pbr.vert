#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;
layout (location = 3) in vec4 aTangent; // xyz=tangent, w=handedness

uniform mat4 uMVP;

out vec3 vNormal;
out vec2 vUV;
out vec3 vTangent;
out float vTangentW;

void main() {
    vNormal = aNormal;
    vUV = aUV;
    vTangent = aTangent.xyz;
    vTangentW = aTangent.w;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
