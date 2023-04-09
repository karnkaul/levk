#pragma once
#include <glm/vec2.hpp>
#include <graphics/vulkan/common.hpp>
#include <levk/graphics/render_device.hpp>
#include <levk/util/flex_array.hpp>
#include <vulkan/vulkan.hpp>
#include <functional>
#include <mutex>
#include <optional>

namespace levk::vulkan {
struct CopyImage {
	Vma::Image const& image;
	vk::ImageLayout layout{};
	glm::ivec2 offset{};
};

struct Framebuffer;
struct Depthbuffer;

struct Device {
	using View = DeviceView;

	struct Renderer {
		virtual ~Renderer() = default;

		Ptr<AssetProviders const> asset_providers{};

		virtual void render_shadow(vk::CommandBuffer cb, Depthbuffer& depthbuffer, glm::vec2 map_size) = 0;
		virtual void render_3d(vk::CommandBuffer cb, Framebuffer& framebuffer, ImageView const& shadow_map) = 0;
		virtual void render_ui(vk::CommandBuffer cb, Framebuffer& framebuffer, ImageView const& output_3d) = 0;
	};

	struct Impl;
	struct Deleter {
		void operator()(Impl const*) const;
	};

	Device(Window const& window, RenderDeviceCreateInfo const& create_info);

	RenderDeviceInfo device_info{};

	vk::UniqueInstance instance{};
	vk::UniqueDebugUtilsMessengerEXT debug{};
	vk::UniqueSurfaceKHR surface{};

	vk::UniqueDevice device{};
	Queue queue{};
	UniqueVma vma{};

	std::unique_ptr<Impl, Deleter> impl{};

	RenderDeviceInfo const& info() const { return device_info; }
	std::uint64_t draw_calls() const;

	bool set_vsync(Vsync desired);
	bool render(Renderer& renderer, AssetProviders const& asset_providers);

	View view();
};
} // namespace levk::vulkan
