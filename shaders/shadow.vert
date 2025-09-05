#version 330 core
layout (location=0) in vec3 aPos;
layout (location=1) in vec3 aNormal;
layout (location=2) in vec2 aUV;
layout (location=3) in vec4 aTangent;

uniform mat4 uLightMVP;

void main(){
    gl_Position = uLightMVP * vec4(aPos,1.0);
}
