#version 450 core

layout (set = 2, binding = 0) uniform samplerCube cubemap;

layout (location = 2) in vec4 in_fpos;

layout (location = 0) out vec4 out_rgba;

// unused
layout (location = 0) in vec4 in_rgba;
layout (location = 1) in vec2 in_uv;

void main() {
	out_rgba = texture(cubemap, vec3(in_fpos));
}
