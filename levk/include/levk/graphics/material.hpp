#pragma once
#include <levk/graphics/shader.hpp>
#include <levk/graphics/texture.hpp>
#include <levk/serializable.hpp>
#include <levk/uri.hpp>
#include <levk/util/ptr.hpp>

namespace levk {
class TextureProvider;

enum class AlphaMode : std::uint32_t { eOpaque = 0, eBlend, eMask };

inline constexpr std::size_t max_textures_v{8};

struct MaterialTextures : Serializable {
	std::array<Uri<Texture>, max_textures_v> uris{};

	std::string_view type_name() const override { return "MaterialTextures"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;
};

class Material : public Serializable {
  public:
	virtual ~Material() = default;

	virtual void write_sets(Shader& shader, TextureProvider const& texture_provider) const = 0;
	virtual Uri<ShaderCode> const& shader_uri() const = 0;
	virtual std::unique_ptr<Material> clone() const = 0;

	Texture const& texture_or(std::size_t info_index, Texture const& fallback) const;

	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;

	MaterialTextures textures{};
	RenderMode render_mode{};
};

using UMaterial = std::unique_ptr<Material>;

class UnlitMaterial : public Material {
  public:
	Rgba tint{white_v};
	Uri<ShaderCode> shader{"shaders/unlit.frag"};
	std::string name{"unlit"};

	void write_sets(Shader& shader, TextureProvider const& provider) const override;
	Uri<ShaderCode> const& shader_uri() const override { return shader; }
	std::unique_ptr<Material> clone() const override { return std::make_unique<UnlitMaterial>(*this); }

	std::string_view type_name() const override { return "UnlitMaterial"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;
};

class LitMaterial : public Material {
  public:
	Rgba albedo{white_v};
	glm::vec3 emissive_factor{0.0f};
	float metallic{0.5f};
	float roughness{0.5f};
	float alpha_cutoff{};
	AlphaMode alpha_mode{AlphaMode::eOpaque};
	Uri<ShaderCode> shader{"shaders/lit.frag"};
	std::string name{"lit"};

	void write_sets(Shader& shader, TextureProvider const& provider) const override;
	Uri<ShaderCode> const& shader_uri() const override { return shader; }
	std::unique_ptr<Material> clone() const override { return std::make_unique<LitMaterial>(*this); }

	std::string_view type_name() const override { return "LitMaterial"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;
};
} // namespace levk
