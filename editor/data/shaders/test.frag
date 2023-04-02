#version 450 core

layout (set = 2, binding = 0) uniform Mat {
	vec4 tint;
};

layout (set = 2, binding = 1) uniform sampler2D tex;

layout (location = 0) in vec2 in_uv;

layout (location = 0) out vec4 out_rgb;

void main() {
	out_rgb = texture(tex, in_uv) * tint;
}
