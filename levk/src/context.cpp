#include <levk/context.hpp>
#include <levk/scene.hpp>

namespace levk {
Context::Context(DataSource const& data_source, UriMonitor& uri_monitor, Serializer const& serializer, Window&& window, GraphicsDevice&& graphics_device)
	: engine(std::move(window), std::move(graphics_device)), providers(engine.get().graphics_device(), data_source, uri_monitor, serializer) {}

void Context::render(Scene const& scene) { engine.get().render(scene, providers.get(), scene.camera, scene.lights); }
} // namespace levk
