#pragma once
#include <levk/shader.hpp>
#include <levk/texture.hpp>
#include <levk/util/monotonic_map.hpp>
#include <levk/util/ptr.hpp>

namespace levk {
struct TextureFallback {
	MonotonicMap<Texture> const& textures;
	Texture const& white;
	Texture const& black;

	Texture const& get_or(Id<Texture> id, Texture const& fallback) const {
		if (auto const* ret = textures.find(id)) { return *ret; }
		return fallback;
	}
};

template <typename Type>
concept MaterialT = requires(Type& t, Type const& ct, Shader& s, TextureFallback const& tf) {
						std::is_copy_constructible_v<Type>;
						{ t.write_sets(s, tf) };
						{ ct.render_mode() } -> std::convertible_to<RenderMode>;
						{ ct.shader_id() } -> std::same_as<std::string const&>;
					};

enum class AlphaMode : std::uint32_t { eOpaque = 0, eBlend, eMask };

class Material {
  public:
	Material(Material&&) = default;
	Material& operator=(Material&&) = default;
	Material(Material const& rhs) : m_model(clone(rhs.m_model)) {}
	Material& operator=(Material const& rhs) { return (m_model = clone(rhs.m_model), *this); }

	template <MaterialT T>
	Material(T&& t) : m_model(std::make_unique<Model<T>>(std::move(t))) {}

	void write_sets(Shader& shader, TextureFallback const& fallback) const { m_model->write_sets(shader, fallback); }
	RenderMode const& render_mode() const { return m_model->render_mode(); }
	std::string const& shader_id() const { return m_model->shader_id(); }

	template <typename T>
	Ptr<T> as() const {
		if (auto* p = dynamic_cast<Model<T>*>(m_model.get())) { return &p->impl; }
		return {};
	}

  private:
	struct Base {
		virtual ~Base() = default;

		virtual void write_sets(Shader& shader, TextureFallback const& texture_fallback) = 0;
		virtual RenderMode const& render_mode() const = 0;
		virtual std::string const& shader_id() const = 0;
		virtual std::unique_ptr<Base> clone() const = 0;
	};

	template <typename T>
	struct Model : Base {
		T t;
		Model(T&& t) : t(std::move(t)) {}
		void write_sets(Shader& shader, TextureFallback const& fallback) final { t.write_sets(shader, fallback); }
		RenderMode const& render_mode() const final { return t.render_mode(); }
		std::string const& shader_id() const final { return t.shader_id(); }
		std::unique_ptr<Base> clone() const final { return std::make_unique<Model<T>>(*this); }
	};

	static std::unique_ptr<Base> clone(std::unique_ptr<Base> const& rhs) {
		if (!rhs) { return {}; }
		return rhs->clone();
	}

	std::unique_ptr<Base> m_model{};
};

struct UnlitMaterial {
	Rgba tint{white_v};
	Id<Texture> texture{};
	RenderMode mode{};
	std::string shader{"shaders/unlit.frag"};

	void write_sets(Shader& shader, TextureFallback const& fallback) const;
	RenderMode const& render_mode() const { return mode; }
	std::string const& shader_id() const { return shader; }
};

struct LitMaterial {
	Rgba albedo{white_v};
	glm::vec3 emissive_factor{0.0f};
	float metallic{0.5f};
	float roughness{0.5f};
	Id<Texture> base_colour{};
	Id<Texture> roughness_metallic{};
	Id<Texture> emissive{};
	RenderMode mode{};
	float alpha_cutoff{};
	AlphaMode alpha_mode{AlphaMode::eOpaque};
	std::string shader{"shaders/lit.frag"};

	void write_sets(Shader& pipeline, TextureFallback const& fallback) const;
	RenderMode const& render_mode() const { return mode; }
	std::string const& shader_id() const { return shader; }
};

static_assert(MaterialT<UnlitMaterial>);
static_assert(MaterialT<LitMaterial>);
} // namespace levk

#include <levk/resource_map.hpp>
#include <levk/serializable.hpp>

namespace levk::refactor {
struct TextureFallback {
	ResourceMap<Texture> const& textures;
	Texture const& white;
	Texture const& black;

	Texture const& get_or(Uri const& uri, Texture const& fallback) const {
		if (auto const* ret = textures.find(uri)) { return *ret; }
		return fallback;
	}
};

enum class AlphaMode : std::uint32_t { eOpaque = 0, eBlend, eMask };

class MaterialBase : public Serializable {
  public:
	struct TextureInfo {
		Uri uri{};
		ColourSpace colour_space{ColourSpace::eSrgb};
	};

	virtual ~MaterialBase() = default;

	virtual void write_sets(Shader& pipeline, TextureFallback const& fallback) const = 0;
	virtual RenderMode const& render_mode() const = 0;
	virtual std::string const& shader_id() const = 0;
	virtual std::unique_ptr<MaterialBase> clone() const = 0;
	virtual std::vector<TextureInfo> texture_infos() const = 0;
};

class Material {
  public:
	Material(Material&&) = default;
	Material& operator=(Material&&) = default;
	Material(Material const& rhs) : m_model(clone(rhs.m_model)) {}
	Material& operator=(Material const& rhs) { return (m_model = clone(rhs.m_model), *this); }

	template <std::derived_from<MaterialBase> T>
	Material(std::unique_ptr<T>&& t) : m_model(std::move(t)) {}

	void write_sets(Shader& shader, TextureFallback const& fallback) const { m_model->write_sets(shader, fallback); }
	RenderMode const& render_mode() const { return m_model->render_mode(); }
	std::string const& shader_id() const { return m_model->shader_id(); }

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
	TUri<Texture> texture{};
	RenderMode mode{};
	std::string shader{"shaders/unlit.frag"};
	std::string name{"unlit"};

	void write_sets(Shader& shader, TextureFallback const& fallback) const override;
	RenderMode const& render_mode() const override { return mode; }
	std::string const& shader_id() const override { return shader; }
	std::unique_ptr<MaterialBase> clone() const override { return std::make_unique<UnlitMaterial>(*this); }

	std::string_view type_name() const override { return "UnlitMaterial"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;
	std::vector<TextureInfo> texture_infos() const override;
};

class LitMaterial : public MaterialBase {
  public:
	Rgba albedo{white_v};
	glm::vec3 emissive_factor{0.0f};
	float metallic{0.5f};
	float roughness{0.5f};
	TUri<Texture> base_colour{};
	TUri<Texture> roughness_metallic{};
	TUri<Texture> emissive{};
	RenderMode mode{};
	float alpha_cutoff{};
	AlphaMode alpha_mode{AlphaMode::eOpaque};
	std::string shader{"shaders/lit.frag"};
	std::string name{"lit"};

	void write_sets(Shader& pipeline, TextureFallback const& fallback) const override;
	RenderMode const& render_mode() const override { return mode; }
	std::string const& shader_id() const override { return shader; }
	std::unique_ptr<MaterialBase> clone() const override { return std::make_unique<LitMaterial>(*this); }

	std::string_view type_name() const override { return "LitMaterial"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;
	std::vector<TextureInfo> texture_infos() const override;
};
} // namespace levk::refactor
