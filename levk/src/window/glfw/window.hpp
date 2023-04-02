#include <levk/util/ptr.hpp>
#include <levk/window/window.hpp>
#include <vulkan/vulkan.hpp>

struct GLFWwindow;

namespace levk::glfw {
struct Window {
	Window(glm::uvec2 extent, char const* title) noexcept(false);
	~Window() noexcept;

	Window& operator=(Window&&) = delete;

	static std::vector<char const*> vulkan_extensions();

	vk::UniqueSurfaceKHR make_surface(vk::Instance instance) const;

	glm::uvec2 framebuffer_extent() const;
	void show();
	void hide();
	void close();
	bool is_open() const;
	void poll();
	WindowState const& state() const;
	char const* clipboard() const;
	void set_clipboard(char const* text);
	CursorMode cursor_mode() const;
	void set_cursor_mode(CursorMode mode);

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
