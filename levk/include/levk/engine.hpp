#pragma once
#include <levk/build_version.hpp>
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
	Window::State const& state;
	Time dt{};
};

class Engine {
  public:
	using CreateInfo = EngineCreateInfo;

	Engine(Engine&&) noexcept;
	Engine& operator=(Engine&&) noexcept;
	~Engine() noexcept;

	explicit Engine(Window&& window, GraphicsDevice&& device, CreateInfo const& create_info = {}) noexcept(false);

	Window& window() const;
	GraphicsDevice& device() const;

	Frame next_frame();
	void render(GraphicsRenderer const& renderer, Camera const& camera, Lights const& lights, Rgba clear = black_v);

	Time delta_time() const;
	int framerate() const;

  private:
	struct Impl;

	std::unique_ptr<Impl> m_impl{};
};
} // namespace levk
