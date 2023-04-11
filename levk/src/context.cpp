#include <levk/context.hpp>
#include <levk/graphics/shapes.hpp>
#include <levk/scene/scene.hpp>
#include <levk/scene/shape_renderer.hpp>
#include <levk/scene/skeleton_controller.hpp>
#include <levk/scene/skinned_mesh_renderer.hpp>
#include <levk/scene/static_mesh_renderer.hpp>

namespace levk {
namespace {
AssetProviders::CreateInfo make_apci(Engine const& engine, NotNull<DataSource const*> data_source, NotNull<Serializer const*> serializer) {
	return AssetProviders::CreateInfo{
		.render_device = &engine.render_device(),
		.font_library = &engine.font_library(),
		.data_source = data_source,
		.serializer = serializer,
	};
}
} // namespace

Context::Context(NotNull<DataSource const*> data_source, Engine::CreateInfo const& create_info)
	: engine(create_info), asset_providers(make_apci(engine.get(), data_source, &serializer.get())), scene_manager(&asset_providers.get()) {

	serializer.get().bind<SkeletonController>();
	serializer.get().bind<StaticMeshRenderer>();
	serializer.get().bind<SkinnedMeshRenderer>();
	serializer.get().bind<SkeletonController>();
	serializer.get().bind<ShapeRenderer>();

	serializer.get().bind<UnlitMaterial>();
	serializer.get().bind<LitMaterial>();
	serializer.get().bind<SkinnedMaterial>();
	serializer.get().bind<Scene>();

	serializer.get().bind<QuadShape>();
	serializer.get().bind<CubeShape>();
	serializer.get().bind<SphereShape>();
}

void Context::render(RenderList render_list) const { scene_manager.get().render(std::move(render_list)); }
} // namespace levk
