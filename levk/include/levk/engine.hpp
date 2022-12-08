#pragma once
#include <levk/graphics_device.hpp>
#include <levk/util/time.hpp>
#include <levk/window.hpp>

namespace levk {
struct EngineCreateInfo {
	glm::uvec2 window_extent{1280u, 720u};
	char const* window_title{"levk"};
	bool autoshow{};
};

struct Frame {
	std::span<Event const> events{};
	Secs dt{};
};

class Engine {
  public:
	using CreateInfo = EngineCreateInfo;

	explicit Engine(Window&& window, GraphicsDevice&& device, CreateInfo const& create_info = {}) noexcept(false);

	Window const& window() const { return m_window; }
	GraphicsDevice const& device() const { return m_device; }

	void show() { m_window.show(); }
	void hide() { m_window.hide(); }
	void shutdown() { m_window.close(); }
	bool is_running() const { return m_window.is_open(); }
	Frame next_frame();
	void render(GraphicsRenderer& renderer) { m_device.render(renderer, m_window.framebuffer_extent()); }

  private:
	Window m_window;
	GraphicsDevice m_device;
	DeltaTime m_dt{};
};
} // namespace levk
