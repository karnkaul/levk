#version 450 core

struct DirLight {
	vec3 direction;
	vec3 ambient;
	vec3 diffuse;
};

layout (location = 0) in vec3 vpos;
layout (location = 1) in vec4 vrgba;
layout (location = 2) in vec3 vnormal;
layout (location = 3) in vec2 vuv;

layout (location = 4) in vec4 imat0;
layout (location = 5) in vec4 imat1;
layout (location = 6) in vec4 imat2;
layout (location = 7) in vec4 imat3;

layout (location = 8) in uvec4 joint;
layout (location = 9) in vec4 weight;

layout (set = 0, binding = 0) uniform VP {
	mat4 mat_vp;
	vec4 vpos_exposure;
	mat4 mat_shadow;
	vec4 shadow_dir;
};

layout (set = 1, binding = 0) readonly buffer DL {
	DirLight dir_lights[];
};

layout (set = 3, binding = 0) readonly buffer JM {
	mat4 mat_joint[];
};

out gl_PerVertex {
	vec4 gl_Position;
};

layout (location = 0) out vec4 out_rgba;
layout (location = 1) out vec2 out_uv;
layout (location = 2) out vec3 out_normal;
layout (location = 3) out vec4 out_fpos;
layout (location = 4) out vec4 out_vpos_exposure;
layout (location = 5) out vec4 out_fpos_shadow;
layout (location = 6) out vec3 out_shadow_dir;

void main() {
	mat4 skin_mat = 
		weight.x * mat_joint[joint.x] +
		weight.y * mat_joint[joint.y] +
		weight.z * mat_joint[joint.z] +
		weight.w * mat_joint[joint.w];

	mat4 mat_m = mat4(
		imat0,
		imat1,
		imat2,
		imat3
	);

	vec4 pos = mat_m * skin_mat * vec4(vpos, 1.0);

	out_rgba = vrgba;
	out_uv = vuv;
	out_normal = normalize(vec3(skin_mat * vec4(vnormal, 0.0)));
	out_vpos_exposure = vpos_exposure;
	out_fpos = pos;
	out_fpos_shadow = mat_shadow * pos;
	out_shadow_dir = vec3(shadow_dir);
	gl_Position = mat_vp * pos;
}
