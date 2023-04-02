#include <graphics/vulkan/ad_hoc_cmd.hpp>
#include <graphics/vulkan/device.hpp>
#include <graphics/vulkan/texture.hpp>
#include <levk/graphics/texture.hpp>
#include <levk/util/logger.hpp>
#include <cmath>

namespace levk {
namespace {
constexpr auto white_image_v = FixedPixelMap<1, 1>{{white_v}};
}

void Texture::Deleter::operator()(vulkan::Texture const* ptr) const { delete ptr; }

Texture::Texture(vulkan::Device& device) : Texture(device, white_image_v.view(), CreateInfo{}) {}

Texture::Texture(vulkan::Device& device, Image::View image, CreateInfo const& create_info) : m_impl(new vulkan::Texture{}) {
	m_impl->device = device.view();
	static constexpr auto magenta_pixmap_v = FixedPixelMap<1, 1>{{magenta_v}};
	m_impl->create_info.format = create_info.colour_space == ColourSpace::eLinear ? vk::Format::eR8G8B8A8Unorm : vk::Format::eR8G8B8A8Srgb;
	bool mip_mapped = create_info.mip_mapped;
	if (image.extent.x == 0 || image.extent.y == 0) {
		logger::warn("[Texture] invalid image extent: [0x0]");
		image = magenta_pixmap_v.view();
		mip_mapped = false;
	}
	if (image.storage.empty()) {
		logger::warn("[Texture] invalid image bytes: [empty]");
		image = magenta_pixmap_v.view();
		mip_mapped = false;
	}
	if (mip_mapped && m_impl->device.can_mip(m_impl->create_info.format)) {
		m_impl->create_info.mip_levels = m_impl->device.compute_mip_levels({image.extent.x, image.extent.y});
	}
	auto vk_image = m_impl->device.vma.make_image(m_impl->create_info, {image.extent.x, image.extent.y});

	auto cmd = vulkan::AdHocCmd{m_impl->device};
	cmd.scratch_buffers.push_back(m_impl->device.vma.copy_to_image(cmd.cb, vk_image.get(), {&image, 1}));
	m_impl->image = {*m_impl->device.defer, std::move(vk_image)};
}

std::uint32_t Texture::mip_levels() const { return m_impl->create_info.mip_levels; }
ColourSpace Texture::colour_space() const { return vulkan::is_srgb(m_impl->create_info.format) ? ColourSpace::eSrgb : ColourSpace::eLinear; }

Extent2D Texture::extent() const {
	auto const view = m_impl->image.get().get().image_view();
	return {view.extent.width, view.extent.height};
}

bool Texture::resize_canvas(Extent2D new_extent, Rgba background, glm::uvec2 top_left) {
	if (new_extent.x == 0 || new_extent.y == 0) { return false; }
	auto const extent_ = extent();
	if (new_extent == extent_) { return true; }
	if (top_left.x + extent_.x > new_extent.x || top_left.y + extent_.y > new_extent.y) { return false; }

	auto const image_type = m_impl->image.get().get().type;
	assert(image_type == vk::ImageViewType::e2D);
	auto pixels = DynPixelMap{new_extent};
	for (Rgba& rgba : pixels.span()) { rgba = background; }
	auto const image_view = pixels.view();
	auto new_image = m_impl->device.vma.make_image(m_impl->create_info, {image_view.extent.x, image_view.extent.y});

	auto cmd = vulkan::AdHocCmd{m_impl->device};
	cmd.scratch_buffers.push_back(m_impl->device.vma.copy_to_image(cmd.cb, new_image.get(), {&image_view, 1}));
	auto src = vulkan::Vma::Copy{m_impl->image.get().get(), vk::ImageLayout::eShaderReadOnlyOptimal};
	auto dst = vulkan::Vma::Copy{new_image.get(), vk::ImageLayout::eShaderReadOnlyOptimal, top_left};
	m_impl->device.vma.copy_image(cmd.cb, src, dst, {extent_.x, extent_.y});
	m_impl->image = {*m_impl->device.defer, std::move(new_image)};

	return true;
}

bool Texture::write(std::span<ImageWrite const> writes) {
	auto const extent_ = extent();
	for (auto const& write : writes) {
		auto const bottom_right = write.offset + write.image.extent;
		if (bottom_right.x > extent_.x || bottom_right.y > extent_.y) { return false; }
	}

	auto cmd = vulkan::AdHocCmd{m_impl->device};
	auto scratch = m_impl->device.vma.write_images(cmd.cb, m_impl->image.get(), writes);
	if (!scratch.get().buffer) { return false; }
	cmd.scratch_buffers.push_back(std::move(scratch));
	return true;
}
} // namespace levk
