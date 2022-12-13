#version 450 core

struct DirLight {
	vec3 direction;
	vec3 ambient;
	vec3 diffuse;
};

const uint ALPHA_OPAQUE = 0;
const uint ALPHA_BLEND = 1;
const uint ALPHA_MASK = 2;

struct Material {
	vec4 albedo;
	vec4 m_r_aco_am;
	vec4 emissive;
};

layout (set = 0, binding = 1) readonly buffer DL {
	DirLight dir_lights[];
};

layout (set = 1, binding = 0) uniform sampler2D base_colour;
layout (set = 1, binding = 1) uniform sampler2D roughness_metallic;
layout (set = 1, binding = 2) uniform sampler2D emissive;

layout (set = 2, binding = 0) uniform M {
	Material material;
};

layout (location = 0) in vec3 in_rgb;
layout (location = 1) in vec2 in_uv;
layout (location = 2) in vec3 in_normal;
layout (location = 3) in vec3 in_fpos;
layout (location = 4) in vec4 in_vpos_exposure;

layout (location = 0) out vec4 out_rgba;

const float pi_v = 3.14159;

float distribution_ggx(vec3 N, vec3 H, float roughness) {
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.0);
	float NdotH2 = NdotH * NdotH;

	float num = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = pi_v * denom * denom;

	return num / denom;
}

float geometry_schlick_ggx(float NdotV, float roughness) {
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;

	float num = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	return num / denom;
}

float geometry_smith(float NdotV, float NdotL, float roughness) {
	float ggx2 = geometry_schlick_ggx(NdotV, roughness);
	float ggx1 = geometry_schlick_ggx(NdotL, roughness);
	return ggx1 * ggx2;
}

vec3 fresnel_schlick(float cos, vec3 F0) { return F0 + (vec3(1.0) - F0) * pow(max(1.0 - cos, 0.0), 5.0); }

vec3 gamc(vec3 a) {
	float exp = 1.0f / 2.2f;
	return vec3(
		pow(a.x, exp),
		pow(a.y, exp),
		pow(a.z, exp)
	);
}

vec3 cook_torrance() {
	float roughness = material.m_r_aco_am.y * texture(roughness_metallic, in_uv).g;
	float metallic = material.m_r_aco_am.x * texture(roughness_metallic, in_uv).b;
	vec3 f0 = mix(vec3(0.04), vec3(material.albedo), metallic);

	vec3 L0 = vec3(0.0);
	vec3 V = normalize(vec3(in_vpos_exposure) - in_fpos);
	vec3 N = in_normal;
	for (int i = 0; i < dir_lights.length(); ++i) {
		DirLight light = dir_lights[i];
		vec3 L = -light.direction;
		vec3 H = normalize(V + L);

		float NdotL = max(dot(N, L), 0.0);
		float NdotV = max(dot(N, V), 0.0);

		float NDF = distribution_ggx(N, H, roughness);
		float G = geometry_smith(NdotV, NdotL, roughness);
		vec3 F = fresnel_schlick(max(dot(H, V), 0.0), f0);

		vec3 kS = F;
		vec3 kD = vec3(1.0) - kS;
		kD *= 1.0 - metallic;

		vec3 num = NDF * kS * G;
		float denom = 4.0 * NdotV * NdotL + 0.0001;
		vec3 spec = num / denom;

		L0 += (kD * vec3(material.albedo) / pi_v + spec) * light.diffuse * max(in_vpos_exposure.w, 0.0) * NdotL;
	}

	vec3 colour = L0;

	colour /= (colour + vec3(1.0));
	return colour;
}

void main() {
	vec4 diffuse = texture(base_colour, in_uv);
	float alpha_cutoff = material.m_r_aco_am.z;
	uint alpha_mode = floatBitsToUint(material.m_r_aco_am.w);
	if (alpha_mode == ALPHA_OPAQUE) {
		diffuse.w = 1.0;
	} else if (alpha_mode == ALPHA_MASK) {
		if (diffuse.w < alpha_cutoff) { discard; }
		diffuse.w = 1.0;
	}
	out_rgba = vec4(cook_torrance(), 1.0) * diffuse + material.emissive * texture(emissive, in_uv);
}
