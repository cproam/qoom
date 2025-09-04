#version 330 core
in vec3 vNormal;
in vec2 vUV;
in vec3 vTangent;
in float vTangentW;

out vec4 FragColor;

uniform sampler2D uBaseColorTex; // unit 0
uniform sampler2D uORMTex;       // unit 1 (R=AO, G=Roughness, B=Metallic)
uniform sampler2D uNormalTex;    // unit 2
uniform int uHasORM;
uniform int uHasNormal;

uniform float uMetallic;
uniform float uRoughness;
uniform vec3 uLightDir;   // direction toward light
uniform vec3 uLightColor;

// Approx dispersion toggle/params
uniform int   uDispersionEnabled;
uniform float uAbbeNumber; // higher -> less dispersion
uniform float uIOR;

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// Build TBN from interpolated attributes; assume bitangent via cross(N,T)*handedness
mat3 makeTBN(vec3 N, vec3 T, float handedness)
{
    vec3 n = normalize(N);
    vec3 t = normalize(T - n * dot(T, n));
    vec3 b = cross(n, t) * handedness;
    return mat3(t, b, n);
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightDir);
    vec3 V = vec3(0,0,1); // view dir in clip space approx
    vec3 H = normalize(L + V);

    vec3 albedo = texture(uBaseColorTex, vUV).rgb;

    float metallic = clamp(uMetallic, 0.0, 1.0);
    float roughness = clamp(uRoughness, 0.04, 1.0);
    float ao = 1.0;
    if (uHasORM == 1) {
        vec3 orm = texture(uORMTex, vUV).rgb;
        ao = orm.r;
        roughness = clamp(orm.g, 0.04, 1.0);
        metallic = clamp(orm.b, 0.0, 1.0);
    }

    // Normal mapping
    if (uHasNormal == 1) {
        vec3 nrm = texture(uNormalTex, vUV).xyz * 2.0 - 1.0;
        mat3 TBN = makeTBN(N, vTangent, vTangentW);
        N = normalize(TBN * nrm);
        H = normalize(L + V);
    }

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = fresnelSchlick(VdotH, F0);
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    // Trowbridge-Reitz (GGX) NDF (simplified)
    float denom = (NdotH * NdotH) * (alpha2 - 1.0) + 1.0;
    float D = alpha2 / (3.14159 * denom * denom + 1e-7);
    // Smith masking-shadowing (Schlick-GGX approx)
    float k = (alpha + 1.0);
    k = (k * k) / 8.0;
    float Gv = NdotV / (NdotV * (1.0 - k) + k);
    float Gl = NdotL / (NdotL * (1.0 - k) + k);
    float G = Gv * Gl;

    vec3 spec = (D * G) * F / max(4.0 * NdotV * NdotL, 1e-4);
    vec3 kd = (1.0 - F) * (1.0 - metallic);
    vec3 diffuse = kd * albedo / 3.14159;

    vec3 color = (diffuse + spec) * uLightColor * NdotL * ao;

    if (uDispersionEnabled == 1) {
        float strength = clamp(50.0 / max(uAbbeNumber, 1.0), 0.0, 1.0);
        color.r *= (uIOR + 0.01 * strength);
        color.g *= (uIOR);
        color.b *= (uIOR - 0.01 * strength);
    }

    FragColor = vec4(color, 1.0);
}
