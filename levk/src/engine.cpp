#include <impl/ft_lib_wrapper.hpp>
#include <levk/component_factory.hpp>
#include <levk/engine.hpp>
#include <levk/scene.hpp>
#include <levk/serializer.hpp>
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
	Window window;
	RenderDevice render_device;
	std::unique_ptr<FontLibrary> font_library{};
	DeltaTime dt{};
	Fps fps{};

	Impl(Window&& window, RenderDevice&& gd) : window(std::move(window)), render_device(std::move(gd)) {}
};

Engine::Engine(Engine&&) noexcept = default;
Engine& Engine::operator=(Engine&&) noexcept = default;

Engine::~Engine() noexcept {
	if (!m_impl) { return; }
	Service<RenderDevice>::unprovide();
	Service<Window>::unprovide();
}

Engine::Engine(Window&& window, RenderDevice&& device, CreateInfo const& create_info) noexcept(false)
	: m_impl(std::make_unique<Impl>(std::move(window), std::move(device))) {
	m_impl->window.create(create_info.window_extent, create_info.window_title);
	m_impl->render_device.create({m_impl->window});
	m_impl->font_library = make_font_library();
	if (!m_impl->font_library->init()) { logger::error("[Engine] Failed to initialize FontLibrary!"); }

	Service<Window>::provide(&m_impl->window);
	Service<RenderDevice>::provide(&m_impl->render_device);

	if (create_info.autoshow) { m_impl->window.show(); }
}

Window& Engine::window() const { return m_impl->window; }
RenderDevice& Engine::render_device() const { return m_impl->render_device; }
FontLibrary const& Engine::font_library() const { return *m_impl->font_library; }

Frame Engine::next_frame() {
	m_impl->window.poll();
	m_impl->fps();
	return {.state = m_impl->window.state(), .dt = m_impl->dt()};
}

void Engine::render(Renderer const& renderer, AssetProviders const& providers, Camera const& camera, Lights const& lights) {
	m_impl->render_device.render(renderer, providers, camera, lights, m_impl->window.framebuffer_extent());
}

Time Engine::delta_time() const { return m_impl->dt.value; }
int Engine::framerate() const { return m_impl->fps.fps == 0 ? m_impl->fps.frames : m_impl->fps.fps; }
} // namespace levk
