#include <levk/asset/asset_io.hpp>
#include <levk/asset/asset_providers.hpp>
#include <levk/level/attachment.hpp>
#include <levk/level/shape.hpp>

namespace levk {
namespace {
auto const g_log{Logger{"AssetProviders"}};
}

AssetProviders::AssetProviders(CreateInfo const& create_info) {
	m_providers.shader = &add(ShaderProvider{create_info.data_source});
	m_providers.skeletal_animation = &add(SkeletalAnimationProvider{create_info.data_source});
	m_providers.skeleton = &add(SkeletonProvider{m_providers.skeletal_animation, create_info.data_source});
	m_providers.texture = &add(TextureProvider{create_info.render_device, create_info.data_source});
	m_providers.cubemap = &add(CubemapProvider{create_info.render_device, create_info.data_source, create_info.thread_pool});
	m_providers.material = &add(MaterialProvider{m_providers.texture, create_info.serializer});
	m_providers.static_mesh = &add(StaticMeshProvider{m_providers.material});
	m_providers.skinned_mesh = &add(SkinnedMeshProvider{m_providers.skeleton, m_providers.material});
	m_providers.ascii_font = &add(AsciiFontProvider{m_providers.texture, create_info.font_library});
	m_providers.pcm = &add(PcmProvider{create_info.data_source});

	m_on_mount_point_changed = create_info.data_source->on_mount_point_changed().connect([this](std::string_view) { clear(); });
}

void AssetProviders::reload_out_of_date() {
	for (auto const& provider : m_storage) { provider->reload_out_of_date(); }
}

void AssetProviders::clear() {
	for (auto const& provider : m_storage) { provider->clear(); }
	g_log.info("All assets cleared");
}

asset::Type AssetProviders::asset_type(Uri<> const& uri) const {
	auto json = data_source().read_json(uri);
	if (!json) { return {}; }
	auto ret = asset::Type{};
	asset::from_json(json, ret);
	return ret;
}

MeshType AssetProviders::mesh_type(Uri<> const& uri) const {
	auto json = data_source().read_json(uri);
	auto asset_type = asset::Type{};
	asset::from_json(json, asset_type);
	if (!json || asset_type != asset::Type::eMesh) { return MeshType::eNone; }
	if (json.contains("skeleton")) { return MeshType::eSkinned; }
	return MeshType::eStatic;
}

AssetList AssetProviders::build_asset_list(Uri<Level> const& uri) const {
	auto json = data_source().read_json(uri);
	auto level = Level{};
	asset::from_json(json, level);
	auto ret = AssetList{};
	if (level.skybox) { ret.cubemaps.insert(level.skybox); }
	for (auto const& [_, attachments] : level.attachments_map) {
		for (auto const& attachment : attachments) {
			auto result = serializer().deserialize_as<Attachment>(attachment);
			if (!result) { continue; }
			result.value->add_assets(ret);
		}
	}
	for (auto const& mesh : ret.meshes) {
		auto asset = asset::Mesh3D{};
		auto json = data_source().read_json(mesh);
		if (!json) { continue; }
		asset::from_json(json, asset);
		for (auto const& in_primitive : asset.primitives) {
			if (in_primitive.material) { ret.materials.insert(in_primitive.material); }
		}
		if (auto const& skeleton = json["skeleton"]) { ret.skeletons.insert(skeleton.as<std::string>()); }
	}
	for (auto const& skeleton : ret.skeletons) {
		auto asset = Skeleton{};
		auto json = data_source().read_json(skeleton);
		if (!json) { continue; }
		asset::from_json(json, asset);
		for (auto const& in_animation : asset.animations) { ret.animations.insert(in_animation); }
	}
	for (auto const& material : ret.materials) {
		auto asset = asset::Material{};
		auto json = data_source().read_json(material);
		if (!json) { continue; }
		asset::from_json(json, asset);
		ret.shaders.insert(asset.vertex_shader);
		ret.shaders.insert(asset.fragment_shader);
		for (auto const& uri : asset.textures.uris) {
			if (uri) { ret.textures.insert(uri); }
		}
	}
	return ret;
}
} // namespace levk
