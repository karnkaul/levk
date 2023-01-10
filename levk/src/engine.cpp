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

void add_serializer_bindings() {
	auto& serializer = Service<Serializer>::locate();
	serializer.bind<SkeletonController>();
	serializer.bind<MeshRenderer>();
	serializer.bind<LitMaterial>();
	serializer.bind<UnlitMaterial>();
}
} // namespace

struct Engine::Impl {
	Window window;
	GraphicsDevice graphics_device;
	Service<Serializer>::Instance serializer{};
	DeltaTime dt{};
	Fps fps{};

	Impl(Window&& window, GraphicsDevice&& gd) : window(std::move(window)), graphics_device(std::move(gd)) {}
};

Engine::Engine(Engine&&) noexcept = default;
Engine& Engine::operator=(Engine&&) noexcept = default;

Engine::~Engine() noexcept {
	if (!m_impl) { return; }
	Service<GraphicsDevice>::unprovide();
	Service<Window>::unprovide();
}

Engine::Engine(Window&& window, GraphicsDevice&& device, CreateInfo const& create_info) noexcept(false)
	: m_impl(std::make_unique<Impl>(std::move(window), std::move(device))) {
	m_impl->window.create(create_info.window_extent, create_info.window_title);
	m_impl->graphics_device.create({m_impl->window});

	Service<Window>::provide(&m_impl->window);
	Service<GraphicsDevice>::provide(&m_impl->graphics_device);

	add_serializer_bindings();

	if (create_info.autoshow) { m_impl->window.show(); }
}

Window const& Engine::window() const { return m_impl->window; }
Window& Engine::window() { return m_impl->window; }
GraphicsDevice const& Engine::device() const { return m_impl->graphics_device; }
GraphicsDevice& Engine::device() { return m_impl->graphics_device; }

void Engine::show() { m_impl->window.show(); }
void Engine::hide() { m_impl->window.hide(); }
void Engine::shutdown() { m_impl->window.close(); }

bool Engine::is_running() const { return m_impl->window.is_open(); }

Frame Engine::next_frame() {
	m_impl->window.poll();
	m_impl->fps();
	return {.state = m_impl->window.state(), .dt = m_impl->dt()};
}

void Engine::render(GraphicsRenderer& renderer, Camera const& camera, Lights const& lights, Rgba clear) {
	m_impl->graphics_device.render(renderer, camera, lights, m_impl->window.framebuffer_extent(), clear);
}

int Engine::framerate() const { return m_impl->fps.fps == 0 ? m_impl->fps.frames : m_impl->fps.fps; }
} // namespace levk
