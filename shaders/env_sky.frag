#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uEnvEquirect; // equirectangular environment (RGB), assumed linear
uniform mat3 uCameraBasis;      // columns: right, up, forward

// Convert direction to equirectangular UV
vec2 dirToEquirect(vec3 d){
    float phi = atan(d.z, d.x);
    float theta = acos(clamp(d.y, -1.0, 1.0));
    float u = (phi / (2.0*3.14159265)) + 0.5;
    u = fract(u);
    float v = theta / 3.14159265;
    vec2 uv = vec2(u, v);
    vec2 texel = 1.0 / vec2(textureSize(uEnvEquirect, 0));
    uv.x = fract(uv.x + 0.5 * texel.x);
    return uv;
}

void main(){
    // reconstruct ray dir in view space from NDC, then to world via camera basis
    vec2 ndc = vUV * 2.0 - 1.0;
    vec3 dirView = normalize(vec3(ndc.x, ndc.y, 1.0));
    vec3 d = normalize(uCameraBasis * dirView);
    vec2 uv = dirToEquirect(d);
    vec3 col = texture(uEnvEquirect, uv).rgb;
    FragColor = vec4(col, 1.0);
}