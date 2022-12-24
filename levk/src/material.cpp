#include <levk/material.hpp>
#include <cstring>

namespace levk {
namespace {
// std::bit_cast not available on GCC 10.x
float bit_cast_f(AlphaMode const mode) {
	auto ret = float{};
	std::memcpy(&ret, &mode, sizeof(mode));
	return ret;
}
} // namespace

void UnlitMaterial::write_sets(Shader& shader, TextureFallback const& fallback) const {
	shader.update(1, 0, fallback.get_or(texture, fallback.white));
	shader.write(2, 0, Rgba::to_srgb(tint.to_vec4()));
}

void LitMaterial::write_sets(Shader& shader, TextureFallback const& fallback) const {
	shader.update(1, 0, fallback.get_or(base_colour, fallback.white));
	shader.update(1, 1, fallback.get_or(roughness_metallic, fallback.white));
	shader.update(1, 2, fallback.get_or(emissive, fallback.black));
	struct MatUBO {
		glm::vec4 albedo;
		glm::vec4 m_r_aco_am;
		glm::vec4 emissive;
	};
	auto const mat = MatUBO{
		.albedo = Rgba::to_linear(albedo.to_vec4()),
		.m_r_aco_am = {metallic, roughness, 0.0f, bit_cast_f(alpha_mode)},
		.emissive = Rgba::to_linear({emissive_factor, 1.0f}),
	};
	shader.write(2, 0, mat);
}
} // namespace levk
