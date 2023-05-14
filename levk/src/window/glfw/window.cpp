#include <levk/util/error.hpp>
#include <window/glfw/window.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace levk::glfw {
std::vector<char const*> Window::vulkan_extensions() {
	auto extension_count = std::uint32_t{};
	auto const extensions = glfwGetRequiredInstanceExtensions(&extension_count);
	if (!extensions) { throw Error{"[Window] Failed to find required Vulkan extensions"}; }
	return {extensions, extensions + extension_count};
}

vk::UniqueSurfaceKHR Window::make_surface(vk::Instance instance) const {
	auto ret = VkSurfaceKHR{};
	auto const res = glfwCreateWindowSurface(static_cast<VkInstance>(instance), window, {}, &ret);
	if (res != VK_SUCCESS || !ret) { throw Error{"[Wsi] Failed to create Vulkan Surface"}; }
	return vk::UniqueSurfaceKHR{ret, instance};
}

glm::uvec2 Window::framebuffer_extent() const {
	auto ret = glm::ivec2{};
	glfwGetFramebufferSize(window, &ret.x, &ret.y);
	return ret;
}

glm::uvec2 Window::window_extent() const {
	auto ret = glm::ivec2{};
	glfwGetWindowSize(window, &ret.x, &ret.y);
	return ret;
}
} // namespace levk::glfw
