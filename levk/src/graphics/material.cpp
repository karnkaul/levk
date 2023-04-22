#include <imgui.h>
#include <graphics/vulkan/material.hpp>
#include <levk/asset/texture_provider.hpp>
#include <levk/graphics/material.hpp>
#include <levk/graphics/shader.hpp>
#include <levk/imcpp/drag_drop.hpp>
#include <levk/io/common.hpp>
#include <levk/util/enumerate.hpp>
#include <levk/util/fixed_string.hpp>

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

void Material::Deleter::operator()(vulkan::Material const* ptr) const { delete ptr; }

Material::Material() : m_impl(new vulkan::Material{}) {}

bool Material::serialize(dj::Json& out) const {
	textures.serialize(out["textures"]);
	to_json(out["render_mode"], render_mode);
	out["vertex_shader"] = vertex_shader.value();
	out["fragment_shader"] = fragment_shader.value();
	return true;
}

bool Material::deserialize(dj::Json const& json) {
	textures.deserialize(json["textures"]);
	from_json(json["render_mode"], render_mode);
	vertex_shader = json["vertex_shader"].as_string();
	fragment_shader = json["fragment_shader"].as_string();
	return true;
}

void Material::inspect(imcpp::OpenWindow) {
	ImGui::Text("%s", FixedString{"Vertex Shader: {}", vertex_shader.value()}.c_str());
	ImGui::Text("%s", FixedString{"Fragment Shader: {}", fragment_shader.value()}.c_str());
	for (auto [texture, index] : enumerate(textures.uris)) {
		if (auto tn = imcpp::TreeNode{FixedString{"texture[{}]", index}.c_str()}) {
			FixedString<128> const label = texture ? texture.value() : "[None]";
			imcpp::TreeNode::leaf(label.c_str());
			if (texture) {
				if (auto drag = imcpp::DragDrop::Source{}) { imcpp::DragDrop::set_string("texture", texture.value()); }
			}
			if (auto drop = imcpp::DragDrop::Target{}) {
				if (auto tex = imcpp::DragDrop::accept_string("texture"); !tex.empty()) { texture = tex; }
			}
		}
	}
}

UnlitMaterial::UnlitMaterial() {
	vertex_shader = "shaders/unlit.vert";
	fragment_shader = "shaders/unlit.frag";
}

void UnlitMaterial::write_sets(Shader& shader, TextureProvider const& provider) const {
	shader.update(2, 0, provider.get(textures.uris[0]));
	shader.write(2, 1, Rgba::to_srgb(tint.to_vec4()));
}

bool UnlitMaterial::serialize(dj::Json& out) const {
	Material::serialize(out);
	to_json(out["tint"], tint);
	return true;
}

bool UnlitMaterial::deserialize(dj::Json const& json) {
	Material::deserialize(json);
	from_json(json["tint"], tint);
	return true;
}

LitMaterial::LitMaterial() {
	vertex_shader = "shaders/lit.vert";
	fragment_shader = "shaders/lit.frag";
}

void LitMaterial::write_sets(Shader& shader, TextureProvider const& provider) const {
	shader.update(2, 0, provider.get(textures.uris[0]));
	shader.update(2, 1, provider.get(textures.uris[1]));
	shader.update(2, 2, provider.get(textures.uris[2], "black"));
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
	shader.write(2, 3, mat);
}

bool LitMaterial::serialize(dj::Json& out) const {
	Material::serialize(out);
	to_json(out["albedo"], albedo);
	to_json(out["emissive_factor"], emissive_factor);
	out["metallic"] = metallic;
	out["roughness"] = roughness;
	out["alpha_cutoff"] = alpha_cutoff;
	out["alpha_mode"] = from(alpha_mode);
	out["name"] = name;
	return true;
}

bool LitMaterial::deserialize(dj::Json const& json) {
	assert(json["type_name"].as_string() == type_name());
	Material::deserialize(json);
	from_json(json["albedo"], albedo);
	from_json(json["emissive_factor"], emissive_factor, emissive_factor);
	metallic = json["metallic"].as<float>(metallic);
	roughness = json["roughness"].as<float>(roughness);
	alpha_cutoff = json["alpha_cutoff"].as<float>(alpha_cutoff);
	alpha_mode = to_alpha_mode(json["alpha_mode"].as_string());
	name = json["name"].as_string();
	return true;
}

SkinnedMaterial::SkinnedMaterial() { vertex_shader = "shaders/skinned.vert"; }
} // namespace levk
