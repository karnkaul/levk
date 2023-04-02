#pragma once
#include <graphics/vulkan/device.hpp>
#include <graphics/vulkan/render_target.hpp>
#include <levk/graphics/camera.hpp>
#include <levk/graphics/lights.hpp>
#include <levk/graphics/render_device.hpp>
#include <levk/graphics/rgba.hpp>

namespace levk::vulkan {
struct RenderCamera {
	static constexpr std::uint32_t view_set_v{0u};

	struct RenderInfo {
		Device::Drawer& drawer;
		BufferView dir_lights_ssbo{};
		std::span<Drawable const> drawables{};
	};

	struct Std140View {
		glm::mat4 mat_vp;
		glm::vec4 vpos_exposure;
	};

	DeviceView device{};
	HostBuffer view_ubo{};

	RenderTarget render_target{};
	Camera camera{};

	static GlobalLayout::Storage make_layout(vk::Device device);
	static RenderCamera make(DeviceView device);

	void bind_view_set(vk::CommandBuffer cb, glm::uvec2 extent);
};
} // namespace levk::vulkan
