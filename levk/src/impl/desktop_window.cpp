#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <levk/impl/desktop_window.hpp>
#include <levk/impl/vulkan_surface.hpp>
#include <levk/util/error.hpp>
#include <levk/window.hpp>
#include <cassert>
#include <unordered_map>

namespace {
levk::DesktopWindow::Storage g_storage{};
} // namespace

void levk::window_create(DesktopWindow& out, glm::uvec2 extent, char const* title) {
	if (!glfwInit() || !glfwVulkanSupported()) { throw levk::Error{"TODO: error"}; }

	auto const iextent = glm::ivec2{extent};
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	out.window = glfwCreateWindow(iextent.x, iextent.y, title, {}, {});
	if (!out.window) { throw levk::Error{"TODO: error"}; }
	out.storage = &g_storage;
	auto extension_count = std::uint32_t{};
	auto const extensions = glfwGetRequiredInstanceExtensions(&extension_count);
	if (!extensions) { throw Error{"TODO: error"}; }
	out.storage->vulkan_extensions = {extensions, extensions + extension_count};

	glfwSetWindowCloseCallback(out.window, [](GLFWwindow*) { g_storage.events.push_back(levk::event::Close{}); });
	glfwSetWindowSizeCallback(out.window, [](GLFWwindow*, int x, int y) {
		g_storage.events.push_back(levk::event::Resize{.extent = {x, y}, .type = event::Resize::Type::eWindow});
	});
	glfwSetFramebufferSizeCallback(out.window, [](GLFWwindow*, int x, int y) {
		g_storage.events.push_back(levk::event::Resize{.extent = {x, y}, .type = event::Resize::Type::eFramebuffer});
	});
	glfwSetWindowFocusCallback(out.window, [](GLFWwindow*, int gained) { g_storage.events.push_back(levk::event::Focus{.gained = gained == GLFW_TRUE}); });
}

void levk::window_destroy(DesktopWindow& out) {
	glfwDestroyWindow(out.window);
	glfwTerminate();
}

std::span<char const*> levk::window_vulkan_extensions(levk::DesktopWindow const&) { return g_storage.vulkan_extensions; }

void levk::window_make_surface(DesktopWindow const& window, Surface& out, Surface::Source const& source) {
	auto& surface = dynamic_cast<VulkanSurface&>(out);
	auto& instance = dynamic_cast<VulkanSurface::Instance const&>(source);
	auto ret = VkSurfaceKHR{};
	auto const res = glfwCreateWindowSurface(instance.instance, window.window, {}, &ret);
	if (res != VK_SUCCESS || !ret) { throw Error{"TODO: error"}; }
	surface.surface = vk::UniqueSurfaceKHR{ret, instance.instance};
}

glm::uvec2 levk::window_framebuffer_extent(DesktopWindow const& window) {
	auto ret = glm::ivec2{};
	glfwGetFramebufferSize(window.window, &ret.x, &ret.y);
	return ret;
}

void levk::window_show(DesktopWindow& out) { glfwShowWindow(out.window); }
void levk::window_hide(DesktopWindow& out) { glfwHideWindow(out.window); }
void levk::window_close(DesktopWindow& out) { glfwSetWindowShouldClose(out.window, GLFW_TRUE); }

bool levk::window_is_open(DesktopWindow const& window) { return !glfwWindowShouldClose(window.window); }

std::span<levk::Event const> levk::window_poll(DesktopWindow&) {
	g_storage.events.clear();
	glfwPollEvents();
	return g_storage.events;
}
