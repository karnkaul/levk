#include <font/lib_wrapper.hpp>
#include <impl/frame_profiler.hpp>
#include <levk/engine.hpp>
#include <levk/io/serializer.hpp>
#include <levk/scene/scene.hpp>
#include <levk/service.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/thread_pool.hpp>

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

auto const g_log{Logger{"Engine"}};
} // namespace

struct Engine::Impl {
	Service<Window>::Instance window;
	Service<RenderDevice>::Instance render_device;
	std::unique_ptr<FontLibrary> font_library;
	ThreadPool thread_pool{};

	Fps fps{};

	Impl(CreateInfo const& create_info)
		: window(create_info.window_extent, create_info.window_title), render_device(window.get(), create_info.render_device_create_info),
		  font_library(make_font_library()) {}
};

void Engine::Deleter::operator()(Impl* ptr) const {
	if (ptr) { ptr->thread_pool.wait_idle(); }
	delete ptr;
}

Engine::Engine(CreateInfo const& create_info) noexcept(false) : m_impl(new Impl{create_info}) {
	if (!m_impl->font_library->init()) { g_log.error("Failed to initialize FontLibrary!"); }
	if (create_info.autoshow) { m_impl->window.get().show(); }
}

Window& Engine::window() const { return m_impl->window.get(); }
RenderDevice& Engine::render_device() const { return m_impl->render_device.get(); }
FontLibrary const& Engine::font_library() const { return *m_impl->font_library; }
ThreadPool& Engine::thread_pool() const { return m_impl->thread_pool; }

void Engine::next_frame() {
	FrameProfiler::instance().profile(FrameProfile::Type::eFrameTime);
	m_impl->window.get().poll();
	m_impl->fps();
}

FrameProfile Engine::frame_profile() const { return FrameProfiler::instance().previous_profile(); }

int Engine::framerate() const { return m_impl->fps.fps == 0 ? m_impl->fps.frames : m_impl->fps.fps; }
} // namespace levk
