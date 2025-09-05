#version 330 core
in vec3 vNormal;
in vec2 vUV;
in vec3 vTangent;
in float vTangentW;
in vec4 vLightSpacePos;
in vec3 vWorldPos;

out vec4 FragColor;

uniform sampler2D uBaseColorTex; // unit 0 (sRGB)
uniform sampler2D uORMTex;       // unit 1 (R=AO, G=Roughness, B=Metallic)
uniform sampler2D uNormalTex;    // unit 2
uniform sampler2D uEnvEquirect;  // unit 4 (linear HDR)
uniform int uHasORM;
uniform int uHasNormal;
uniform sampler2D uRoughnessTex; // unit 5 (optional separate roughness)
uniform sampler2D uMetalnessTex; // unit 6 (optional separate metalness)
uniform int uHasRoughness;
uniform int uHasMetalness;

uniform vec3 uLightDir;   // direction from light to scene (sun direction)
uniform vec3 uLightColor; // intensity (can be >1)
uniform vec3 uAmbientColor;
uniform vec4 uBaseColorFactor; // material base color factor
uniform sampler2DShadow uShadowMap;
uniform mat4 uLightBias; // usually bias * lightMVP in vertex, but we’ll compute here with vLightSpacePos
uniform vec3 uCameraPos;
uniform float uMetallicFactor;   // from material
uniform float uRoughnessFactor;  // from material
uniform float uEnvSpecStrength;  // scale for specular IBL
uniform float uEnvDiffStrength;  // scale for diffuse IBL
uniform float uOverrideRoughness; // < 0 to ignore, else force [0,1]
uniform float uOverrideMetallic;  // < 0 to ignore, else force [0,1]
// Box projection controls (for voxel visualization)
uniform int uUseBoxUVMapping;     // 0 = use mesh UVs, 1 = box-project using world pos
uniform float uBoxUVScale;        // tiles per meter (e.g., 1.0 = 1 repeat per meter)

const float PI = 3.14159265;

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

mat3 makeTBN(vec3 N, vec3 T, float handedness)
{
	vec3 n = normalize(N);
	vec3 t = normalize(T - n * dot(T, n));
	vec3 b = cross(n, t) * handedness;
	return mat3(t, b, n);
}

	// Convert direction to equirectangular UV
	vec2 dirToEquirect(vec3 d){
		float phi = atan(d.z, d.x);
		float theta = acos(clamp(d.y, -1.0, 1.0));
		float u = phi / (2.0*PI) + 0.5;
		u = fract(u); // wrap to [0,1)
		vec2 uv = vec2(u, theta / PI);
		// half-texel offset on U to avoid border sampling causing seams
		vec2 texel = 1.0 / vec2(textureSize(uEnvEquirect, 0));
		uv.x = fract(uv.x + 0.5 * texel.x);
		return uv;
	}

void main() {
	vec3 N = normalize(vNormal);
	vec3 L = normalize(-uLightDir);
	vec3 V = normalize(uCameraPos - vWorldPos);
	vec3 H = normalize(L + V);

	// Choose UVs: mesh UVs or box-projected from world position
	vec2 baseUV = vUV;
	if (uUseBoxUVMapping == 1) {
		vec3 an = abs(N);
		if (an.x >= an.y && an.x >= an.z) {
			baseUV = vWorldPos.zy;
		} else if (an.y >= an.x && an.y >= an.z) {
			baseUV = vWorldPos.xz;
		} else {
			baseUV = vWorldPos.xy;
		}
		baseUV *= uBoxUVScale;
	}

	vec3 albedo = texture(uBaseColorTex, baseUV).rgb * uBaseColorFactor.rgb;
	float metallic = clamp(uMetallicFactor, 0.0, 1.0);
	float roughness = clamp(uRoughnessFactor, 0.04, 1.0);
	float ao = 1.0;
	if (uHasORM == 1) {
		vec3 orm = texture(uORMTex, vUV).rgb;
		ao = orm.r;
		roughness = clamp(orm.g * uRoughnessFactor, 0.04, 1.0);
		metallic = clamp(orm.b * uMetallicFactor, 0.0, 1.0);
	}
	else {
		if (uHasRoughness == 1) {
			float r = texture(uRoughnessTex, vUV).r;
			roughness = clamp(r * uRoughnessFactor, 0.04, 1.0);
		}
		if (uHasMetalness == 1) {
			float m = texture(uMetalnessTex, vUV).r;
			metallic = clamp(m * uMetallicFactor, 0.0, 1.0);
		}
	}

	// Optional global overrides for quick tuning of untextured assets
	if (uOverrideRoughness >= 0.0) {
		roughness = clamp(uOverrideRoughness, 0.04, 1.0);
	}
	if (uOverrideMetallic >= 0.0) {
		metallic = clamp(uOverrideMetallic, 0.0, 1.0);
	}

	if (uHasNormal == 1) {
		vec3 nrm = texture(uNormalTex, vUV).xyz * 2.0 - 1.0;
		mat3 TBN = makeTBN(N, vTangent, vTangentW);
		N = normalize(TBN * nrm);
		H = normalize(L + V);
	}

	float NdotL = max(dot(N, L), 0.0);
	// Shadow: transform to shadow map space (bias matrix is 0.5* + 0.5)
	vec3 projCoords = vLightSpacePos.xyz / max(vLightSpacePos.w, 1e-6);
	projCoords = projCoords * 0.5 + 0.5;
	// Percentage-closer filtering (3x3)
	float shadow = 0.0;
	if (projCoords.z <= 1.0) {
		// dynamic slope-scale bias using geometric normal to reduce acne while preserving contact shadows
		float NdotL_geo = max(dot(normalize(vNormal), L), 0.0);
		float dynamicBias = max(0.0005 * (1.0 - NdotL_geo), 0.00005);
		float texel = 1.0 / textureSize(uShadowMap, 0).x;
		for (int x = -1; x <= 1; ++x) {
			for (int y = -1; y <= 1; ++y) {
				vec2 offs = vec2(x, y) * texel;
				shadow += texture(uShadowMap, vec3(projCoords.xy + offs, projCoords.z - dynamicBias));
			}
		}
		shadow /= 9.0;
	} else {
		shadow = 1.0;
	}
	float NdotV = max(dot(N, V), 0.0);
	float VdotH = max(dot(V, H), 0.0);
	float alphaR = roughness * roughness;
	float alpha2 = alphaR * alphaR;

	vec3 F0 = mix(vec3(0.04), albedo, metallic);
	vec3 F = fresnelSchlick(VdotH, F0);
	float NdotH = max(dot(N, H), 0.0);
	float denom = (NdotH * NdotH) * (alpha2 - 1.0) + 1.0;
	float D = alpha2 / (PI * denom * denom + 1e-7);
	float k = (alphaR + 1.0);
	k = (k * k) / 8.0;
	float Gv = NdotV / (NdotV * (1.0 - k) + k);
	float Gl = NdotL / (NdotL * (1.0 - k) + k);
	float G = Gv * Gl;

	vec3 spec = (D * G) * F / max(4.0 * NdotV * NdotL, 1e-4);
	vec3 kd = (1.0 - F) * (1.0 - metallic);
	vec3 diffuse = kd * albedo / PI;

	vec3 color = (diffuse + spec) * uLightColor * NdotL * ao * shadow + uAmbientColor * albedo * ao;

	// Simple IBL: sample environment for diffuse and specular components
	vec2 uvN = dirToEquirect(N);
	// Cross-fade across seam for diffuse sample
	float seamW = 1.5 / float(textureSize(uEnvEquirect, 0).x); // ~1.5 texels
	vec3 envDiff;
	if (uvN.x < seamW) {
		vec3 c0 = texture(uEnvEquirect, uvN).rgb;
		vec3 c1 = texture(uEnvEquirect, vec2(uvN.x + 1.0, uvN.y)).rgb;
		float t = smoothstep(0.0, seamW, uvN.x);
		envDiff = mix(c1, c0, t);
	} else if (uvN.x > 1.0 - seamW) {
		vec3 c0 = texture(uEnvEquirect, uvN).rgb;
		vec3 c1 = texture(uEnvEquirect, vec2(uvN.x - 1.0, uvN.y)).rgb;
		float t = smoothstep(1.0, 1.0 - seamW, uvN.x);
		envDiff = mix(c1, c0, t);
	} else {
		envDiff = texture(uEnvEquirect, uvN).rgb;
	}
	float kd_ibl = (1.0 - metallic); // simple energy term for IBL
	vec3 diffuseIBL = kd_ibl * (albedo / PI) * envDiff * uEnvDiffStrength;
	vec3 R = reflect(-V, N);
	vec2 uvR = dirToEquirect(R);
	vec3 envSpec;
	if (uvR.x < seamW) {
		vec3 c0 = texture(uEnvEquirect, uvR).rgb;
		vec3 c1 = texture(uEnvEquirect, vec2(uvR.x + 1.0, uvR.y)).rgb;
		float t = smoothstep(0.0, seamW, uvR.x);
		envSpec = mix(c1, c0, t);
	} else if (uvR.x > 1.0 - seamW) {
		vec3 c0 = texture(uEnvEquirect, uvR).rgb;
		vec3 c1 = texture(uEnvEquirect, vec2(uvR.x - 1.0, uvR.y)).rgb;
		float t = smoothstep(1.0, 1.0 - seamW, uvR.x);
		envSpec = mix(c1, c0, t);
	} else {
		envSpec = texture(uEnvEquirect, uvR).rgb;
	}
	vec3 specColor = mix(vec3(0.05), albedo, metallic); // slightly boosted dielectric F0
	float gloss = pow(1.0 - roughness, 2.2);
	float dielectricBoost = mix(1.8, 1.0, metallic); // boost dielectrics so reflections are visible
	vec3 specIBL = envSpec * specColor * gloss * dielectricBoost * uEnvSpecStrength;
	color += (diffuseIBL * (1.0 - metallic) + specIBL) * ao;
	FragColor = vec4(color, 1.0);
}
