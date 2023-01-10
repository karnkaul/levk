#include <levk/asset/common.hpp>
#include <levk/material.hpp>
#include <cstring>

namespace levk {
namespace {
constexpr AlphaMode to_alpha_mode(std::string_view const str) {
	if (str == "blend") { return AlphaMode::eBlend; }
	if (str == "mask") { return AlphaMode::eMask; }
	return AlphaMode::eOpaque;
}

constexpr std::string_view from(AlphaMode const mode) {
	switch (mode) {
	case AlphaMode::eBlend: return "blend";
	case AlphaMode::eMask: return "mask";
	default: return "opaque";
	}
}

// std::bit_cast not available on GCC 10.x
template <typename AlphaModeT>
float bit_cast_f(AlphaModeT const mode) {
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

// TODO
bool UnlitMaterial::serialize(dj::Json& out) const { return false; }

// TODO
bool UnlitMaterial::deserialize(dj::Json const& json) { return false; }

std::vector<MaterialBase::TextureInfo> UnlitMaterial::texture_infos() const {
	return {
		{texture},
	};
}

bool LitMaterial::serialize(dj::Json& out) const {
	asset::to_json(out["albedo"], albedo);
	asset::to_json(out["emissive_factor"], emissive_factor);
	out["metallic"] = metallic;
	out["roughness"] = roughness;
	if (base_colour) { out["base_colour"] = base_colour.value(); }
	if (roughness_metallic) { out["roughness_metallic"] = roughness_metallic.value(); }
	if (emissive) { out["emissive"] = emissive.value(); }
	out["alpha_cutoff"] = alpha_cutoff;
	out["alpha_mode"] = from(alpha_mode);
	out["shader"] = shader;
	out["name"] = name;
	return true;
}

bool LitMaterial::deserialize(dj::Json const& json) {
	assert(json["type_name"].as_string() == type_name());
	asset::from_json(json["albedo"], albedo);
	asset::from_json(json["emissive_factor"], emissive_factor, emissive_factor);
	metallic = json["metallic"].as<float>(metallic);
	roughness = json["roughness"].as<float>(roughness);
	base_colour = json["base_colour"].as<std::string>();
	roughness_metallic = json["roughness_metallic"].as<std::string>();
	emissive = json["emissive"].as<std::string>();
	alpha_cutoff = json["alpha_cutoff"].as<float>(alpha_cutoff);
	alpha_mode = to_alpha_mode(json["alpha_mode"].as_string());
	shader = json["shader"].as_string(shader);
	name = json["name"].as_string();
	return true;
}

std::vector<MaterialBase::TextureInfo> LitMaterial::texture_infos() const {
	return {
		{base_colour},
		{roughness_metallic, ColourSpace::eLinear},
		{emissive},
	};
}
} // namespace levk
