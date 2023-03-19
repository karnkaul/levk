#include <levk/context.hpp>
#include <levk/scene.hpp>

namespace levk {
namespace {
AssetProviders::CreateInfo make_apci(Engine const& engine, DataSource const& data_source, UriMonitor& uri_monitor, Serializer const& serializer) {
	return AssetProviders::CreateInfo{
		.render_device = engine.render_device(),
		.font_library = engine.font_library(),
		.data_source = data_source,
		.uri_monitor = uri_monitor,
		.serializer = serializer,
	};
}
} // namespace

Context::Context(DataSource const& data_source, UriMonitor& uri_monitor, Serializer const& serializer, Window&& window, RenderDevice&& render_device)
	: engine(std::move(window), std::move(render_device)), providers(make_apci(engine.get(), data_source, uri_monitor, serializer)) {}

void Context::render(Scene const& scene) { engine.get().render(scene, providers.get(), scene.camera, scene.lights); }
} // namespace levk
