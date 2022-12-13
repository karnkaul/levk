#pragma once
#include <levk/graphics_device.hpp>
#include <levk/util/time.hpp>
#include <levk/window.hpp>

namespace levk {
struct EngineCreateInfo {
	glm::uvec2 window_extent{1280u, 720u};
	glm::vec3 camera_position{0.0f, 0.0f, 5.0f};
	char const* window_title{"levk"};
	bool autoshow{};
};

struct Frame {
	Window::State const& state;
	Time dt{};
};

class Engine {
  public:
	using CreateInfo = EngineCreateInfo;

	explicit Engine(Window&& window, GraphicsDevice&& device, CreateInfo const& create_info = {}) noexcept(false);

	Window const& window() const { return m_window; }
	GraphicsDevice const& device() const { return m_device; }
	GraphicsDevice& device() { return m_device; }

	void show() { m_window.show(); }
	void hide() { m_window.hide(); }
	void shutdown() { m_window.close(); }
	bool is_running() const { return m_window.is_open(); }
	Frame next_frame();
	void render(GraphicsRenderer& renderer, Rgba clear = black_v) { m_device.render(renderer, camera, lights, m_window.framebuffer_extent(), clear); }

	Camera camera{};
	Lights lights{};

  private:
	Window m_window;
	GraphicsDevice m_device;
	DeltaTime m_dt{};
};
} // namespace levk
