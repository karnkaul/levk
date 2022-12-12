#pragma once
#include <levk/rgba.hpp>
#include <levk/shader.hpp>
#include <levk/texture.hpp>

namespace levk {
struct TextureFallback {
	Texture const& white;
	Texture const& black;
};

struct Material {
	class Instance;

	virtual ~Material() = default;

	virtual void write_sets(Shader& shader, TextureFallback const& texture_fallback) = 0;
};

struct UnlitMaterial : Material {
	Rgba tint{};
	Ptr<Texture const> texture{};

	void write_sets(Shader& shader, TextureFallback const& fallback) final;
};

class Material::Instance {
  public:
	template <std::derived_from<Material> T>
	Instance(T&& t) : m_material(std::make_unique<T>(std::move(t))) {}

	void write_sets(Shader& shader, TextureFallback const& fallback) const { m_material->write_sets(shader, fallback); }

	template <typename T>
	Ptr<T> as() const {
		return dynamic_cast<T*>(m_material.get());
	}

  private:
	std::unique_ptr<Material> m_material{};
};
} // namespace levk
