#include <levk/engine.hpp>
#include <levk/scene.hpp>
#include <levk/serializer.hpp>
#include <levk/service.hpp>

namespace levk {
namespace {
void add_serializer_bindings() {
	auto& serializer = Service<Serializer>::locate();
	serializer.bind_to<SkeletonController>("SkeletonController");
	serializer.bind_to<MeshRenderer>("MeshRenderer");
}
} // namespace

struct Engine::Impl {
	Window window;
	GraphicsDevice graphics_device;
	Service<Serializer>::Instance serializer{};
	DeltaTime dt{};

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

	camera.transform.set_position(create_info.camera_position);

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
	return {.state = m_impl->window.state(), .dt = m_impl->dt()};
}

void Engine::render(GraphicsRenderer& renderer, Rgba clear) {
	m_impl->graphics_device.render(renderer, camera, lights, m_impl->window.framebuffer_extent(), clear);
}
} // namespace levk
