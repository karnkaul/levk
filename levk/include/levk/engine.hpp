#pragma once
#include <levk/build_version.hpp>
#include <levk/graphics/render_device.hpp>
#include <levk/util/time.hpp>
#include <levk/window/window.hpp>

namespace levk {
struct FontLibrary;

struct EngineCreateInfo {
	glm::uvec2 window_extent{1280u, 720u};
	char const* window_title{"levk"};
	bool autoshow{};

	RenderDeviceCreateInfo render_device_create_info{};
};

struct Frame {
	Window::State const& state;
	Time dt{};
};

class Engine {
  public:
	using CreateInfo = EngineCreateInfo;

	explicit Engine(CreateInfo const& create_info = {}) noexcept(false);

	Window& window() const;
	RenderDevice& render_device() const;
	FontLibrary const& font_library() const;

	Frame next_frame();

	Time delta_time() const;
	int framerate() const;

  private:
	struct Impl;

	struct Deleter {
		void operator()(Impl const* ptr) const;
	};

	std::unique_ptr<Impl, Deleter> m_impl{};
};
} // namespace levk
