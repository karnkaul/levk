#pragma once
#include <levk/graphics/common.hpp>
#include <levk/graphics/rgba.hpp>
#include <levk/graphics/shader_code.hpp>
#include <levk/io/serializable.hpp>
#include <levk/level/inspectable.hpp>
#include <levk/uri.hpp>
#include <levk/util/ptr.hpp>
#include <memory>

namespace levk {
struct Shader;

namespace vulkan {
struct Material;
}

class Texture;
class TextureProvider;

enum class AlphaMode : std::uint32_t { eOpaque = 0, eBlend, eMask };

inline constexpr std::size_t max_textures_v{8};

struct MaterialTextures : Serializable {
	std::array<Uri<Texture>, max_textures_v> uris{};

	std::string_view type_name() const override { return "MaterialTextures"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;

	bool operator==(MaterialTextures const& rhs) const { return uris == rhs.uris; }
};

class Material : public Serializable, public Inspectable {
  public:
	virtual ~Material() = default;

	Material();

	Ptr<vulkan::Material> vulkan_material() const { return m_impl.get(); }
	virtual void write_sets(Shader& shader, TextureProvider const& texture_provider) const = 0;
	virtual bool is_opaque() const { return false; }

	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;
	void inspect(imcpp::OpenWindow w) override;

	Uri<ShaderCode> vertex_shader{};
	Uri<ShaderCode> fragment_shader{};
	MaterialTextures textures{};
	RenderMode render_mode{};

  private:
	struct Deleter {
		void operator()(vulkan::Material const* ptr) const;
	};

	std::unique_ptr<vulkan::Material, Deleter> m_impl{};
};

using UMaterial = std::unique_ptr<Material>;

class UnlitMaterial : public Material {
  public:
	UnlitMaterial();

	void write_sets(Shader& shader, TextureProvider const& provider) const override;
	bool is_opaque() const override { return tint.channels[3] == 0xff; }

	std::string_view type_name() const override { return "UnlitMaterial"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;

	Rgba tint{white_v};
};

class LitMaterial : public Material {
  public:
	LitMaterial();

	void write_sets(Shader& shader, TextureProvider const& provider) const override;
	bool is_opaque() const override { return alpha_mode == AlphaMode::eOpaque; }

	std::string_view type_name() const override { return "LitMaterial"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;

	Rgba albedo{white_v};
	glm::vec3 emissive_factor{0.0f};
	float metallic{0.5f};
	float roughness{0.5f};
	float alpha_cutoff{};
	AlphaMode alpha_mode{AlphaMode::eOpaque};
	std::string name{"lit"};
};

class SkinnedMaterial : public LitMaterial {
  public:
	SkinnedMaterial();

	std::string_view type_name() const override { return "SkinnedMaterial"; }
};
} // namespace levk
