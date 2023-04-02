#include <font/lib_wrapper.hpp>
#include <levk/engine.hpp>
#include <levk/io/component_factory.hpp>
#include <levk/io/serializer.hpp>
#include <levk/scene/scene.hpp>
#include <levk/service.hpp>

namespace levk {
namespace {
struct Fps {
	Clock::time_point start{Clock::now()};
	int frames{};
	int fps{};

	void operator()() {
		auto const now = Clock::now();
		++frames;
		if (now - start >= std::chrono::seconds{1}) {
			fps = frames;
			frames = 0;
			start = now;
		}
	}
};
} // namespace

struct Engine::Impl {
	Service<Window>::Instance window;
	Service<RenderDevice>::Instance render_device;
	std::unique_ptr<FontLibrary> font_library{};

	DeltaTime dt{};
	Fps fps{};

	Impl(CreateInfo const& create_info)
		: window(create_info.window_extent, create_info.window_title), render_device(window.get(), create_info.render_device_create_info),
		  font_library(make_font_library()) {}
};

void Engine::Deleter::operator()(Impl const* ptr) const { delete ptr; }

Engine::Engine(CreateInfo const& create_info) noexcept(false) : m_impl(new Impl{create_info}) {
	if (!m_impl->font_library->init()) { logger::error("[Engine] Failed to initialize FontLibrary!"); }
	if (create_info.autoshow) { m_impl->window.get().show(); }
}

Window& Engine::window() const { return m_impl->window.get(); }
RenderDevice& Engine::render_device() const { return m_impl->render_device.get(); }
FontLibrary const& Engine::font_library() const { return *m_impl->font_library; }

Frame Engine::next_frame() {
	m_impl->window.get().poll();
	m_impl->fps();
	return {.state = m_impl->window.get().state(), .dt = m_impl->dt()};
}

Time Engine::delta_time() const { return m_impl->dt.value; }
int Engine::framerate() const { return m_impl->fps.fps == 0 ? m_impl->fps.frames : m_impl->fps.fps; }
} // namespace levk
