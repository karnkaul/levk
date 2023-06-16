#version 450 core

layout (location = 0) in vec3 vpos;
layout (location = 1) in vec3 vrgb;
layout (location = 2) in vec3 vnormal;
layout (location = 3) in vec2 vuv;

layout (location = 4) in vec4 imat0;
layout (location = 5) in vec4 imat1;
layout (location = 6) in vec4 imat2;
layout (location = 7) in vec4 imat3;

layout (set = 0, binding = 0) uniform VP {
	mat4 mat_vp;
};

layout (location = 0) out vec3 out_rgb;
layout (location = 1) out vec2 out_uv;
layout (location = 2) out vec4 out_fpos;

out gl_PerVertex {
	vec4 gl_Position;
};

void main() {
	mat4 mat_m = mat4(
		imat0,
		imat1,
		imat2,
		imat3
	);
	vec4 pos = vec4(vpos, 1.0);
	out_rgb = vrgb;
	out_uv = vuv;
	out_fpos = mat_m * pos;
	gl_Position = mat_vp * out_fpos;
}
