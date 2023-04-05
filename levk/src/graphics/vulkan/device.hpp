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

struct PipelineBuilder;
struct Material;
struct Framebuffer;

struct Device {
	using View = DeviceView;

	struct Drawer;

	struct Impl;
	struct Deleter {
		void operator()(Impl const*) const;
	};

	struct Waiter {
		DeviceView device{};

		~Waiter();
	};

	Device(Window const& window, RenderDeviceCreateInfo const& create_info);

	RenderDeviceInfo device_info{};

	vk::UniqueInstance instance{};
	vk::UniqueDebugUtilsMessengerEXT debug{};
	vk::UniqueSurfaceKHR surface{};

	Gpu gpu{};
	vk::UniqueDevice device{};
	Queue queue{};

	UniqueVma vma{};
	DeferQueue defer{};
	Index buffered_index{};
	RenderMode default_render_mode{};
	GlobalLayout::Storage global_layout{};

	std::unique_ptr<Impl, Deleter> impl{};
	Waiter waiter{};

	std::uint64_t draw_calls{};

	RenderDeviceInfo const& info() const { return device_info; }

	bool set_vsync(Vsync desired);
	bool render(RenderDevice::Frame const& t);

	void render_3d(glm::vec4 clear, RenderDevice::Frame const& frame, Framebuffer& framebuffer, BufferView dir_lights);
	void render_ui(ImageView const& output_3d, RenderDevice::Frame const& frame, Framebuffer& framebuffer, BufferView dir_lights);
	void draw_3d_to_ui(ImageView const& output_3d, PipelineBuilder& pipeline_builder, vk::Extent2D extent);

	View view();
};

struct Device::Drawer {
	Device& device;
	AssetProviders const& asset_providers;
	PipelineBuilder& pipeline_builder;
	vk::Extent2D extent;
	BufferView dir_lights_ssbo;
	vk::CommandBuffer cb;

	Ptr<Material const> previous_material{};

	void draw(Drawable const& drawable);
};
} // namespace levk::vulkan
