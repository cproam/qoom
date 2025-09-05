#version 330 core
// Fullscreen triangle without VBO
out vec2 vUV;

void main(){
    // Fullscreen triangle trick
    // gl_VertexID = 0,1,2 → positions: (-1,-1), (3,-1), (-1,3)
    // See: NVidia/Intel sample for screen-space triangle
    vUV = vec2((gl_VertexID == 1) ? 2.0 : 0.0,
               (gl_VertexID == 2) ? 2.0 : 0.0);
    vec2 pos = vUV * 2.0 - 1.0;
    gl_Position = vec4(pos, 0.0, 1.0);
}