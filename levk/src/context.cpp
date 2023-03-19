#include <levk/context.hpp>
#include <levk/scene.hpp>

namespace levk {
Context::Context(DataSource const& data_source, UriMonitor& uri_monitor, Serializer const& serializer, Window&& window, RenderDevice&& render_device)
	: engine(std::move(window), std::move(render_device)), providers(engine.get().render_device(), data_source, uri_monitor, serializer) {}

void Context::render(Scene const& scene) { engine.get().render(scene, providers.get(), scene.camera, scene.lights); }
} // namespace levk
