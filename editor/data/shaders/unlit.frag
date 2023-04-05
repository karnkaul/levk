#version 450 core

layout (set = 2, binding = 0) uniform sampler2D tex;

layout (set = 2, binding = 1) uniform Tint {
	vec4 tint;
};

layout (location = 0) in vec3 in_rgb;
layout (location = 1) in vec2 in_uv;

layout (location = 0) out vec4 out_rgba;

void main() {
	out_rgba = texture(tex, in_uv) * vec4(in_rgb, 1.0) * tint;
}
