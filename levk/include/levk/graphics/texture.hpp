#pragma once
#include <levk/graphics/common.hpp>
#include <levk/graphics/image.hpp>
#include <levk/graphics/texture_sampler.hpp>
#include <levk/rect.hpp>
#include <levk/util/ptr.hpp>
#include <memory>

namespace levk {
namespace vulkan {
struct Texture;
struct Device;
} // namespace vulkan

struct TextureCreateInfo {
	std::string name{"(Unnamed)"};
	bool mip_mapped{true};
	ColourSpace colour_space{ColourSpace::eSrgb};
	TextureSampler sampler{};
};

class Texture {
  public:
	using CreateInfo = TextureCreateInfo;
	using Write = ImageWrite;
	using Sampler = TextureSampler;

	explicit Texture(vulkan::Device& device);
	Texture(vulkan::Device& device, Image::View image, CreateInfo const& create_info);

	ColourSpace colour_space() const;
	std::uint32_t mip_levels() const;
	Extent2D extent() const;
	std::string_view name() const { return m_name; }

	bool resize_canvas(Extent2D new_extent, Rgba background = black_v, glm::uvec2 top_left = {});

	bool write(Image::View image, glm::uvec2 offset = {});
	bool write(std::span<ImageWrite const> writes);

	Ptr<vulkan::Texture> vulkan_texture() const { return m_impl.get(); }

	Sampler sampler{};

  private:
	struct Deleter {
		void operator()(vulkan::Texture const* ptr) const;
	};

	std::unique_ptr<vulkan::Texture, Deleter> m_impl{};
	std::string m_name{};
};
} // namespace levk
