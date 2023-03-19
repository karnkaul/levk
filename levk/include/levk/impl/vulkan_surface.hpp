#pragma once
#include <levk/graphics/surface.hpp>
#include <vulkan/vulkan.hpp>

namespace levk {
struct VulkanSurface : Surface {
	struct Instance : Surface::Source {
		vk::Instance instance{};
	};

	vk::UniqueSurfaceKHR surface{};
};
} // namespace levk
