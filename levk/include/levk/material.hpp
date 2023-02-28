#pragma once
#include <levk/serializable.hpp>
#include <levk/shader.hpp>
#include <levk/texture.hpp>
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

class MaterialBase : public Serializable {
  public:
	virtual ~MaterialBase() = default;

	virtual void write_sets(Shader& shader, TextureProvider const& texture_provider) const = 0;
	virtual RenderMode const& render_mode() const = 0;
	virtual Uri<ShaderCode> const& shader_id() const = 0;
	virtual std::unique_ptr<MaterialBase> clone() const = 0;

	Texture const& texture_or(std::size_t info_index, Texture const& fallback) const;

	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;

	MaterialTextures textures{};
};

class Material {
  public:
	Material(Material&&) = default;
	Material& operator=(Material&&) = default;
	Material(Material const& rhs) : m_model(clone(rhs.m_model)) {}
	Material& operator=(Material const& rhs) { return (m_model = clone(rhs.m_model), *this); }

	template <std::derived_from<MaterialBase> T>
	Material(std::unique_ptr<T>&& t) : m_model(std::move(t)) {}

	void write_sets(Shader& shader, TextureProvider const& provider) const { m_model->write_sets(shader, provider); }
	RenderMode const& render_mode() const { return m_model->render_mode(); }
	Uri<ShaderCode> const& shader_id() const { return m_model->shader_id(); }

	template <std::derived_from<MaterialBase> T>
	Ptr<T> as() const {
		return dynamic_cast<T*>(m_model.get());
	}

  private:
	static std::unique_ptr<MaterialBase> clone(std::unique_ptr<MaterialBase> const& rhs) {
		if (!rhs) { return {}; }
		return rhs->clone();
	}

	std::unique_ptr<MaterialBase> m_model{};
};

class UnlitMaterial : public MaterialBase {
  public:
	Rgba tint{white_v};
	RenderMode mode{};
	Uri<ShaderCode> shader{"shaders/unlit.frag"};
	std::string name{"unlit"};

	void write_sets(Shader& shader, TextureProvider const& provider) const override;
	RenderMode const& render_mode() const override { return mode; }
	Uri<ShaderCode> const& shader_id() const override { return shader; }
	std::unique_ptr<MaterialBase> clone() const override { return std::make_unique<UnlitMaterial>(*this); }

	std::string_view type_name() const override { return "UnlitMaterial"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;
};

class LitMaterial : public MaterialBase {
  public:
	Rgba albedo{white_v};
	glm::vec3 emissive_factor{0.0f};
	float metallic{0.5f};
	float roughness{0.5f};
	RenderMode mode{};
	float alpha_cutoff{};
	AlphaMode alpha_mode{AlphaMode::eOpaque};
	Uri<ShaderCode> shader{"shaders/lit.frag"};
	std::string name{"lit"};

	void write_sets(Shader& shader, TextureProvider const& provider) const override;
	RenderMode const& render_mode() const override { return mode; }
	Uri<ShaderCode> const& shader_id() const override { return shader; }
	std::unique_ptr<MaterialBase> clone() const override { return std::make_unique<LitMaterial>(*this); }

	std::string_view type_name() const override { return "LitMaterial"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;
};
} // namespace levk
