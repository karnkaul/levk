#version 450 core

layout (location = 0) out vec2 out_uv;

out gl_PerVertex {
	vec4 gl_Position;
};

void main() {
	vec2 uvs[6] = {
		vec2(0.0, 0.0),
		vec2(1.0, 0.0),
		vec2(1.0, 1.0),
		vec2(1.0, 1.0),
		vec2(0.0, 1.0),
		vec2(0.0, 0.0)
	};
	out_uv = uvs[gl_VertexIndex];
	vec2 y_flipped = vec2(2.0 * out_uv - 1.0);
	gl_Position = vec4(y_flipped.x, -y_flipped.y, 0.0, 1.0);
}
