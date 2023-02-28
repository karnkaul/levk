#include <levk/asset/common.hpp>
#include <levk/asset/texture_provider.hpp>
#include <levk/material.hpp>
#include <levk/service.hpp>
#include <levk/util/enumerate.hpp>
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
} // namespace

bool MaterialTextures::serialize(dj::Json& out) const {
	for (auto [uri, index] : enumerate(uris)) {
		if (!uri) { continue; }
		auto& out_info = out.push_back({});
		out_info["index"] = index;
		out_info["uri"] = uri.value();
	}
	return true;
}

bool MaterialTextures::deserialize(dj::Json const& json) {
	for (auto const& in_info : json.array_view()) {
		auto const index = in_info["index"].as<std::size_t>();
		if (index >= uris.size()) { continue; }
		uris[index] = in_info["uri"].as<std::string>();
	}
	return true;
}

bool MaterialBase::serialize(dj::Json& out) const {
	textures.serialize(out["textures"]);
	return true;
}

bool MaterialBase::deserialize(dj::Json const& json) {
	textures.deserialize(json["textures"]);
	return true;
}

void UnlitMaterial::write_sets(Shader& shader, TextureProvider const& provider) const {
	shader.update(1, 0, provider.get(textures.uris[0]));
	shader.write(2, 0, Rgba::to_srgb(tint.to_vec4()));
}

void LitMaterial::write_sets(Shader& shader, TextureProvider const& provider) const {
	shader.update(1, 0, provider.get(textures.uris[0]));
	shader.update(1, 1, provider.get(textures.uris[1]));
	shader.update(1, 2, provider.get(textures.uris[2], "black"));
	struct MatUBO {
		glm::vec4 albedo;
		glm::vec4 m_r_aco_am;
		glm::vec4 emissive;
	};
	auto const mat = MatUBO{
		.albedo = Rgba::to_linear(albedo.to_vec4()),
		.m_r_aco_am = {metallic, roughness, 0.0f, std::bit_cast<float>(alpha_mode)},
		.emissive = Rgba::to_linear({emissive_factor, 1.0f}),
	};
	shader.write(2, 0, mat);
}

bool UnlitMaterial::serialize(dj::Json& out) const {
	MaterialBase::serialize(out);
	asset::to_json(out["tint"], tint);
	asset::to_json(out["render_mode"], mode);
	out["shader"] = shader.value();
	out["name"] = name;
	return true;
}

bool UnlitMaterial::deserialize(dj::Json const& json) {
	MaterialBase::deserialize(json);
	asset::from_json(json["tint"], tint);
	asset::from_json(json["render_mode"], mode);
	shader = json["shader"].as<std::string>();
	name = json["name"].as<std::string>();
	return true;
}

bool LitMaterial::serialize(dj::Json& out) const {
	MaterialBase::serialize(out);
	asset::to_json(out["albedo"], albedo);
	asset::to_json(out["emissive_factor"], emissive_factor);
	asset::to_json(out["render_mode"], mode);
	out["metallic"] = metallic;
	out["roughness"] = roughness;
	out["alpha_cutoff"] = alpha_cutoff;
	out["alpha_mode"] = from(alpha_mode);
	out["shader"] = shader.value();
	out["name"] = name;
	return true;
}

bool LitMaterial::deserialize(dj::Json const& json) {
	assert(json["type_name"].as_string() == type_name());
	MaterialBase::deserialize(json);
	asset::from_json(json["albedo"], albedo);
	asset::from_json(json["emissive_factor"], emissive_factor, emissive_factor);
	asset::from_json(json["render_mode"], mode);
	metallic = json["metallic"].as<float>(metallic);
	roughness = json["roughness"].as<float>(roughness);
	alpha_cutoff = json["alpha_cutoff"].as<float>(alpha_cutoff);
	alpha_mode = to_alpha_mode(json["alpha_mode"].as_string());
	shader = json["shader"].as_string(shader.value());
	name = json["name"].as_string();
	return true;
}
} // namespace levk
