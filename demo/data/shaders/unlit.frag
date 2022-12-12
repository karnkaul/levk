#version 450 core

layout (set = 1, binding = 0) uniform sampler2D tex;

layout (set = 2, binding = 0) uniform Tint {
	vec4 tint;
};

layout (location = 0) in vec3 in_rgb;
layout (location = 1) in vec2 in_uv;
layout (location = 2) in vec3 in_normal;

layout (location = 0) out vec4 out_rgba;

// unused
layout (location = 3) in vec3 in_fpos;
layout (location = 4) in vec4 in_vpos_exposure;

void main() {
	out_rgba = texture(tex, in_uv) * vec4(in_rgb, 1.0) * tint;
}
