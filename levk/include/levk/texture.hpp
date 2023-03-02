#pragma once
#include <levk/graphics_common.hpp>
#include <levk/image.hpp>
#include <levk/rect.hpp>
#include <levk/texture_sampler.hpp>
#include <levk/util/ptr.hpp>
#include <memory>

namespace levk {
struct TextureCreateInfo {
	std::string name{"(Unnamed)"};
	bool mip_mapped{true};
	ColourSpace colour_space{ColourSpace::eSrgb};
	TextureSampler sampler{};
};

class Texture {
  public:
	using CreateInfo = TextureCreateInfo;

	template <typename T>
	Texture(T t, std::string name = "(Unnamed)") : m_model(std::make_unique<Model<T>>(std::move(t))), m_name(std::move(name)) {}

	TextureSampler const& sampler() const { return m_model->sampler(); }
	ColourSpace colour_space() const { return m_model->colour_space(); }
	std::uint32_t mip_levels() const { return m_model->mip_levels(); }
	Extent2D extent() const { return m_model->extent(); }
	std::string_view name() const { return m_name; }

	bool resize_canvas(Extent2D new_extent, Rgba background = black_v, glm::uvec2 top_left = {}) {
		return m_model->resize_canvas(new_extent, background, top_left);
	}

	bool write(Image::View image, glm::uvec2 offset = {}) { return m_model->write(image, offset); }

	template <typename T>
	Ptr<T> as() const {
		if (auto* p = dynamic_cast<Model<T>*>(m_model.get())) { return &p->impl; }
		return {};
	}

  private:
	struct Base {
		virtual ~Base() = default;

		virtual TextureSampler const& sampler() const = 0;
		virtual ColourSpace colour_space() const = 0;
		virtual std::uint32_t mip_levels() const = 0;
		virtual Extent2D extent() const = 0;

		virtual bool resize_canvas(Extent2D new_extent, Rgba background, glm::uvec2 top_left) = 0;
		virtual bool write(Image::View image, glm::uvec2 offset) = 0;
	};

	template <typename T>
	struct Model : Base {
		T impl;
		Model(T&& t) : impl(std::move(t)) {}

		TextureSampler const& sampler() const final { return gfx_tex_sampler(impl); }
		ColourSpace colour_space() const final { return gfx_tex_colour_space(impl); }
		std::uint32_t mip_levels() const final { return gfx_tex_mip_levels(impl); }
		Extent2D extent() const final { return gfx_tex_extent(impl); }

		bool resize_canvas(Extent2D new_extent, Rgba background, glm::uvec2 top_left) final {
			return gfx_tex_resize_canvas(impl, new_extent, background, top_left);
		}

		bool write(Image::View const image, glm::uvec2 const offset) final { return gfx_tex_write(impl, image, offset); }
	};

	std::unique_ptr<Base> m_model{};
	std::string m_name{};
};
} // namespace levk
