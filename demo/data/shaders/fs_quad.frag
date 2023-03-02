#version 450 core

layout (set = 0, binding = 0) uniform sampler2D tex;

layout (location = 0) in vec2 in_uv;

layout (location = 0) out vec4 out_rgba;

void main() {
	out_rgba = vec4(vec3(texture(tex, in_uv)), 1.0);
}
