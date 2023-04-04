#include <levk/context.hpp>
#include <levk/scene/scene.hpp>

namespace levk {
namespace {
AssetProviders::CreateInfo make_apci(Engine const& engine, NotNull<DataSource const*> data_source, NotNull<UriMonitor*> uri_monitor,
									 NotNull<Serializer const*> serializer) {
	return AssetProviders::CreateInfo{
		.render_device = &engine.render_device(),
		.font_library = &engine.font_library(),
		.data_source = data_source,
		.uri_monitor = uri_monitor,
		.serializer = serializer,
	};
}
} // namespace

Context::Context(NotNull<DataSource const*> data_source, Engine::CreateInfo const& create_info)
	: uri_monitor(std::string{data_source->mount_point()}), engine(create_info),
	  asset_providers(make_apci(engine.get(), data_source, &uri_monitor.get(), &serializer.get())), scene_manager(&asset_providers.get()) {

	component_factory.get().bind<SkeletonController>();
	component_factory.get().bind<MeshRenderer>();

	serializer.get().bind<UnlitMaterial>();
	serializer.get().bind<LitMaterial>();
	serializer.get().bind<SkinnedMaterial>();
	serializer.get().bind<Scene>();
}

void Context::render() const {
	auto const& scene = scene_manager.get().active_scene();
	auto render_list = RenderList{};
	scene.render(render_list);
	auto const frame = RenderDevice::Frame{
		.render_list = &render_list,
		.asset_providers = &asset_providers.get(),
		.lights = &scene.lights,
		.camera_3d = &scene.camera,
	};
	engine.get().render_device().render(frame);
}
} // namespace levk
