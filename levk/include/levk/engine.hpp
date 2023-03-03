#pragma once
#include <levk/build_version.hpp>
#include <levk/graphics_device.hpp>
#include <levk/util/time.hpp>
#include <levk/window.hpp>

namespace levk {
struct FontLibrary;

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
	GraphicsDevice& graphics_device() const;
	FontLibrary const& font_library() const;

	Frame next_frame();
	void render(Renderer const& renderer, AssetProviders const& providers, Camera const& camera, Lights const& lights);

	Time delta_time() const;
	int framerate() const;

  private:
	struct Impl;

	std::unique_ptr<Impl> m_impl{};
};
} // namespace levk
