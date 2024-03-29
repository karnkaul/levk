#pragma once
#include <capo/device.hpp>
#include <levk/build_version.hpp>
#include <levk/frame_profile.hpp>
#include <levk/graphics/render_device.hpp>
#include <levk/util/time.hpp>
#include <levk/window/window.hpp>

namespace levk {
struct FontLibrary;
class ThreadPool;
class Scene;

struct EngineCreateInfo {
	glm::uvec2 window_extent{1280u, 720u};
	char const* window_title{"levk"};
	bool autoshow{};

	RenderDeviceCreateInfo render_device_create_info{};
};

class Engine {
  public:
	using CreateInfo = EngineCreateInfo;

	explicit Engine(CreateInfo const& create_info = {}) noexcept(false);

	Window& window() const;
	RenderDevice& render_device() const;
	capo::Device& audio_device() const;
	FontLibrary const& font_library() const;
	ThreadPool& thread_pool() const;

	void next_frame();
	FrameProfile frame_profile() const;

	int framerate() const;

  private:
	struct Impl;

	struct Deleter {
		void operator()(Impl* ptr) const;
	};

	std::unique_ptr<Impl, Deleter> m_impl{};
};
} // namespace levk
