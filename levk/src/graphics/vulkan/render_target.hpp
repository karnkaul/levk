#pragma once
#include <graphics/vulkan/framebuffer.hpp>

namespace levk::vulkan {
struct RenderTarget {
	struct CreateInfo {
		vk::Extent2D extent;

		vk::Format colour{vk::Format::eR8G8B8A8Unorm};
		vk::Format depth{};
		vk::SampleCountFlagBits samples{vk::SampleCountFlagBits::e1};
	};

	DeviceView device{};
	CreateInfo create_info{};
	UniqueImage colour{};
	UniqueImage depth{};
	UniqueImage resolve{};

	static RenderTarget make_backbuffer(DeviceView const& device, CreateInfo const& create_info) {
		auto ret = RenderTarget{.device = device, .create_info = create_info};
		if (create_info.samples > vk::SampleCountFlagBits::e1) {
			assert(create_info.colour != vk::Format{});
			ret.colour = ret.make_colour_target();
		}
		if (create_info.depth != vk::Format{}) { ret.depth = ret.make_depth_target(); }
		return ret;
	}

	static RenderTarget make_off_screen(DeviceView const& device, CreateInfo const& create_info) {
		auto ret = RenderTarget{.device = device, .create_info = create_info};
		assert(create_info.colour != vk::Format{});
		ret.colour = ret.make_colour_target();
		if (create_info.depth != vk::Format{}) { ret.depth = ret.make_depth_target(); }
		if (create_info.samples > vk::SampleCountFlagBits::e1) { ret.resolve = ret.make_resolve_target(); }
		return ret;
	}

	Framebuffer framebuffer() const {
		return {
			.colour = colour.get().image_view(),
			.depth = depth.get().image_view(),
			.resolve = resolve.get().image_view(),
			.samples = create_info.samples,
		};
	}

	Framebuffer refresh(vk::Extent2D extent) {
		if (create_info.extent == extent) { return framebuffer(); }
		create_info.extent = extent;
		if (colour) {
			device.defer->push(std::move(colour));
			colour = make_colour_target();
		}
		if (depth) {
			device.defer->push(std::move(depth));
			depth = make_depth_target();
		}
		if (resolve) {
			device.defer->push(std::move(resolve));
			resolve = make_resolve_target();
		}
		return framebuffer();
	}

	UniqueImage make_colour_target() const {
		auto const ici = ImageCreateInfo{
			.format = create_info.colour,
			.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst |
					 vk::ImageUsageFlagBits::eTransferSrc,
			.aspect = vk::ImageAspectFlagBits::eColor,
			.samples = create_info.samples,
		};
		return device.vma.make_image(ici, create_info.extent);
	}

	UniqueImage make_depth_target() const {
		auto const ici = ImageCreateInfo{
			.format = create_info.depth,
			.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
			.aspect = vk::ImageAspectFlagBits::eDepth,
			.samples = create_info.samples,
		};
		return device.vma.make_image(ici, create_info.extent);
	}

	UniqueImage make_resolve_target() const {
		auto const ici = ImageCreateInfo{
			.format = create_info.colour,
			.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc,
			.aspect = vk::ImageAspectFlagBits::eColor,
			.samples = vk::SampleCountFlagBits::e1,
		};
		return device.vma.make_image(ici, create_info.extent);
	}
};

struct DepthTarget {
	struct CreateInfo {
		vk::Extent2D extent;
		vk::Format format;

		vk::SampleCountFlagBits samples{vk::SampleCountFlagBits::e1};
	};

	DeviceView device{};
	CreateInfo create_info{};
	UniqueImage depth{};

	static DepthTarget make(DeviceView const& device, CreateInfo const& create_info) {
		auto ret = DepthTarget{.device = device, .create_info = create_info};
		assert(create_info.format != vk::Format{});
		ret.depth = ret.make_target();
		return ret;
	}

	UniqueImage make_target() const {
		auto const ici = ImageCreateInfo{
			.format = create_info.format,
			.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
			.aspect = vk::ImageAspectFlagBits::eDepth,
			.samples = create_info.samples,
		};
		return device.vma.make_image(ici, create_info.extent);
	}

	Depthbuffer depthbuffer() const {
		return {
			.image = depth.get().image_view(),
			.samples = create_info.samples,
		};
	}

	Depthbuffer refresh(vk::Extent2D extent) {
		if (create_info.extent == extent) { return depthbuffer(); }
		create_info.extent = extent;
		device.defer->push(std::move(depth));
		depth = make_target();
		return depthbuffer();
	}
};
} // namespace levk::vulkan
