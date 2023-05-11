#include <graphics/vulkan/ad_hoc_cmd.hpp>
#include <graphics/vulkan/device.hpp>
#include <graphics/vulkan/texture.hpp>
#include <levk/graphics/texture.hpp>
#include <levk/util/logger.hpp>
#include <cmath>

namespace levk {
namespace {
constexpr auto white_image_v = FixedPixelMap<1, 1>{{white_v}};

std::array<Image::View, 6> const& white_cubemap() {
	static auto const ret = [] {
		auto ret = std::array<Image::View, 6>{};
		for (auto& view : ret) { view = white_image_v.view(); }
		return ret;
	}();
	return ret;
}

auto const g_log{Logger{"Texture"}};

bool init_texture(vulkan::Texture& out_texture, TextureCreateInfo const& create_info, std::span<Image::View const> images, bool mip_mapped) {
	out_texture.create_info.format = create_info.colour_space == ColourSpace::eLinear ? vk::Format::eR8G8B8A8Unorm : vk::Format::eR8G8B8A8Srgb;
	assert(!images.empty());
	bool first{true};
	auto extent = Extent2D{};
	for (auto const& image : images) {
		if (image.extent.x == 0 || image.extent.y == 0 || image.storage.empty()) { return false; }
		if (first) {
			extent = image.extent;
		} else if (image.extent != extent) {
			return false;
		}
	}
	if (mip_mapped && out_texture.device.can_mip(out_texture.create_info.format)) {
		out_texture.create_info.mip_levels = out_texture.device.compute_mip_levels({extent.x, extent.y});
	}
	out_texture.create_info.array_layers = static_cast<std::uint32_t>(images.size());
	auto const image_view_type = out_texture.create_info.array_layers == 6u ? vk::ImageViewType::eCube : vk::ImageViewType::e2D;
	auto vk_image = out_texture.device.vma.make_image(out_texture.create_info, {extent.x, extent.y}, image_view_type);

	auto cmd = vulkan::AdHocCmd{out_texture.device};
	cmd.scratch_buffers.push_back(out_texture.device.vma.copy_to_image(cmd.cb, vk_image.get(), images));
	out_texture.image = {*out_texture.device.defer, std::move(vk_image)};
	return true;
}
} // namespace

void Texture::Deleter::operator()(vulkan::Texture const* ptr) const { delete ptr; }

Texture::Texture() : m_impl(new vulkan::Texture{}) {}

Texture::Texture(vulkan::Device& device) : Texture(device, white_image_v.view(), CreateInfo{}) {}

Texture::Texture(vulkan::Device& device, Image::View image, CreateInfo const& create_info) : Texture() {
	assert(m_impl);
	m_impl->device = device.view();
	static constexpr auto magenta_pixmap_v = FixedPixelMap<1, 1>{{magenta_v}};
	bool mip_mapped = create_info.mip_mapped;
	if (image.extent.x == 0 || image.extent.y == 0) {
		g_log.warn("invalid image extent: [0x0]");
		image = magenta_pixmap_v.view();
		mip_mapped = false;
	}
	if (image.storage.empty()) {
		g_log.warn("invalid image bytes: [empty]");
		image = magenta_pixmap_v.view();
		mip_mapped = false;
	}
	if (!init_texture(*m_impl, create_info, {&image, 1u}, mip_mapped)) {
		g_log.error("Texture creation failed!");
		image = white_image_v.view();
		init_texture(*m_impl, create_info, {&image, 1u}, false);
	}
}

std::uint32_t Texture::mip_levels() const { return m_impl->create_info.mip_levels; }
ColourSpace Texture::colour_space() const { return vulkan::is_srgb(m_impl->create_info.format) ? ColourSpace::eSrgb : ColourSpace::eLinear; }

Extent2D Texture::extent() const {
	auto const view = m_impl->image.get().get().image_view();
	return {view.extent.width, view.extent.height};
}

TextureSampler const& Texture::sampler() const {
	assert(m_impl);
	return m_impl->sampler;
}

void Texture::set_sampler(Sampler value) {
	if (!m_impl) { return; }
	m_impl->sampler = value;
}

Cubemap::Cubemap(vulkan::Device& device) : Cubemap(device, white_cubemap(), {}) {}

Cubemap::Cubemap(vulkan::Device& device, std::array<Image::View, 6> const& images, CreateInfo const& create_info) {
	assert(m_impl);
	m_impl->device = device.view();
	if (!init_texture(*m_impl, create_info, images, create_info.mip_mapped)) {
		g_log.error("Cubemap creation failed!");
		init_texture(*m_impl, create_info, white_cubemap(), false);
	}
}
} // namespace levk
