#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;
layout (location = 3) in vec4 aTangent;

uniform mat4 uMVP;
uniform mat4 uLightMVP;
uniform mat4 uModel;
uniform mat3 uNormalMatrix;

out vec3 vNormal;
out vec2 vUV;
out vec3 vTangent;
out float vTangentW;
out vec4 vLightSpacePos;
out vec3 vWorldPos;

void main() {
	vNormal = normalize(uNormalMatrix * aNormal);
	vUV = aUV;
	vTangent = normalize(uNormalMatrix * aTangent.xyz);
	vTangentW = aTangent.w;
	vec4 worldPos = uModel * vec4(aPos, 1.0);
	vLightSpacePos = uLightMVP * worldPos;
	vWorldPos = worldPos.xyz;
	gl_Position = uMVP * worldPos;
}
