#include <levk/util/ptr.hpp>
#include <levk/window/window.hpp>
#include <vulkan/vulkan.hpp>

struct GLFWwindow;

namespace levk::glfw {
struct Window {
	static std::vector<char const*> vulkan_extensions();

	vk::UniqueSurfaceKHR make_surface(vk::Instance instance) const;
	glm::uvec2 framebuffer_extent() const;
	glm::uvec2 window_extent() const;

	struct Storage {
		std::vector<std::string> drops{};
		WindowState state{};
		glm::vec2 raw_cursor_position{};
		Ptr<Window> instance{};
	};

	Ptr<Storage> storage{};
	Ptr<GLFWwindow> window{};
};
} // namespace levk::glfw
