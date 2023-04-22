#version 450 core

layout (location = 0) in vec3 vpos;

layout (location = 4) in vec4 imat0;
layout (location = 5) in vec4 imat1;
layout (location = 6) in vec4 imat2;
layout (location = 7) in vec4 imat3;

layout (set = 0, binding = 0) uniform VP {
	mat4 mat_vp;
};

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
	gl_Position = mat_vp * mat_m * vec4(vpos, 1.0);
}
