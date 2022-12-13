#pragma once
#include <levk/shader.hpp>
#include <levk/texture.hpp>
#include <levk/util/ptr.hpp>

namespace levk {
struct TextureFallback {
	Texture const& white;
	Texture const& black;
};

template <typename Type>
concept MaterialT = requires(Type& t, Type const& ct, Shader& s, TextureFallback const& tf) {
						{ t.write_sets(s, tf) };
						{ ct.pipeline_state() } -> std::convertible_to<PipelineState>;
						{ ct.shader_id() } -> std::same_as<std::string const&>;
					};

enum class AlphaMode : std::uint32_t { eOpaque = 0, eBlend, eMask };

class Material {
  public:
	template <MaterialT T>
	Material(T&& t) : m_model(std::make_unique<Model<T>>(std::move(t))) {}

	void write_sets(Shader& shader, TextureFallback const& fallback) const { m_model->write_sets(shader, fallback); }
	PipelineState const& pipeline_state() const { return m_model->pipeline_state(); }
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
		virtual PipelineState const& pipeline_state() const = 0;
		virtual std::string const& shader_id() const = 0;
	};

	template <typename T>
	struct Model : Base {
		T t;
		Model(T&& t) : t(std::move(t)) {}
		void write_sets(Shader& shader, TextureFallback const& fallback) final { t.write_sets(shader, fallback); }
		PipelineState const& pipeline_state() const final { return t.pipeline_state(); }
		std::string const& shader_id() const final { return t.shader_id(); }
	};

	std::unique_ptr<Base> m_model{};
};

struct UnlitMaterial {
	Rgba tint{white_v};
	Ptr<Texture const> texture{};
	PipelineState state{};
	std::string shader{"shaders/unlit.frag"};

	void write_sets(Shader& shader, TextureFallback const& fallback) const;
	PipelineState const& pipeline_state() const { return state; }
	std::string const& shader_id() const { return shader; }
};

struct LitMaterial {
	Rgba albedo{white_v};
	glm::vec3 emissive_factor{0.0f};
	float metallic{0.5f};
	float roughness{0.5f};
	Ptr<Texture const> base_colour{};
	Ptr<Texture const> roughness_metallic{};
	Ptr<Texture const> emissive{};
	PipelineState state{};
	float alpha_cutoff{};
	AlphaMode alpha_mode{AlphaMode::eOpaque};
	std::string shader{"shaders/lit.frag"};

	void write_sets(Shader& pipeline, TextureFallback const& fallback) const;
	PipelineState const& pipeline_state() const { return state; }
	std::string const& shader_id() const { return shader; }
};
} // namespace levk
