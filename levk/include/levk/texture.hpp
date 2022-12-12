#pragma once
#include <levk/graphics_common.hpp>
#include <levk/image.hpp>
#include <levk/util/ptr.hpp>
#include <memory>

namespace levk {
struct Sampler {
	enum class Wrap : std::uint8_t { eRepeat, eClampEdge, eClampBorder };
	enum class Filter : std::uint8_t { eLinear, eNearest };

	Wrap wrap_s{Wrap::eRepeat};
	Wrap wrap_t{Wrap::eRepeat};
	Filter min{Filter::eLinear};
	Filter mag{Filter::eLinear};

	bool operator==(Sampler const&) const = default;
};

struct TextureCreateInfo {
	std::string name{"(Unnamed)"};
	bool mip_mapped{true};
	ColourSpace colour_space{ColourSpace::eSrgb};
	Sampler sampler{};
};

class Texture {
  public:
	using CreateInfo = TextureCreateInfo;

	template <typename T>
	Texture(T t) : m_model(std::make_unique<Model<T>>(std::move(t))) {}

	Sampler const& sampler() const { return m_model->sampler(); }

	template <typename T>
	Ptr<T> as() const {
		if (auto* p = dynamic_cast<Model<T>*>(m_model.get())) { return &p->impl; }
		return {};
	}

	Texture const& or_self(Ptr<Texture const> other) const { return other ? *other : *this; }

  private:
	struct Base {
		virtual ~Base() = default;

		virtual Sampler const& sampler() const = 0;
		virtual ColourSpace colour_space() const = 0;
		virtual std::uint32_t mip_levels() const = 0;
	};

	template <typename T>
	struct Model : Base {
		T impl;
		Model(T&& t) : impl(std::move(t)) {}

		Sampler const& sampler() const final { return gfx_tex_sampler(impl); }
		ColourSpace colour_space() const final { return gfx_tex_colour_space(impl); }
		std::uint32_t mip_levels() const final { return gfx_tex_mip_levels(impl); }
	};

	std::unique_ptr<Base> m_model{};
};
} // namespace levk
