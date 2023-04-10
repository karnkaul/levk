#include <levk/asset/asset_providers.hpp>
#include <levk/asset/common.hpp>
#include <levk/scene/component.hpp>
#include <levk/scene/scene.hpp>

namespace levk {
AssetProviders::AssetProviders(CreateInfo const& create_info) {
	m_providers.shader = &add(ShaderProvider{create_info.data_source});
	m_providers.skeleton = &add(SkeletonProvider{create_info.data_source});
	m_providers.texture = &add(TextureProvider{create_info.render_device, create_info.data_source});
	m_providers.material = &add(MaterialProvider{m_providers.texture, create_info.serializer});
	m_providers.static_mesh = &add(StaticMeshProvider{m_providers.material});
	m_providers.skinned_mesh = &add(SkinnedMeshProvider{m_providers.skeleton, m_providers.material});
	m_providers.ascii_font = &add(AsciiFontProvider{m_providers.texture, create_info.font_library});
}

void AssetProviders::reload_out_of_date() {
	for (auto const& provider : m_storage) { provider->reload_out_of_date(); }
}

void AssetProviders::clear() {
	for (auto const& provider : m_storage) { provider->clear(); }
}

std::string AssetProviders::asset_type(Uri<> const& uri) const {
	auto json = data_source().read_json(uri);
	if (!json) { return {}; }
	return json["asset_type"].as<std::string>();
}

MeshType AssetProviders::mesh_type(Uri<> const& uri) const {
	auto json = data_source().read_json(uri);
	if (!json || json["asset_type"].as_string() != "mesh") { return MeshType::eNone; }
	if (json.contains("skeleton")) { return MeshType::eSkinned; }
	return MeshType::eStatic;
}

AssetList AssetProviders::build_asset_list(Uri<Scene> const& uri) const {
	auto json = data_source().read_json(uri);
	auto ret = Scene::peek_assets(serializer(), json);
	for (auto const& mesh : ret.meshes) {
		auto asset = asset::Mesh3D{};
		auto json = data_source().read_json(mesh);
		asset::from_json(json, asset);
		for (auto const& in_primitive : asset.primitives) {
			if (in_primitive.material) { ret.materials.insert(in_primitive.material); }
		}
		if (auto const& skeleton = json["skeleton"]) { ret.skeletons.insert(skeleton.as<std::string>()); }
	}
	for (auto const& skeleton : ret.skeletons) {
		auto asset = asset::Skeleton{};
		auto json = data_source().read_json(skeleton);
		asset::from_json(json, asset);
		for (auto const& in_animation : asset.animations) { ret.animations.insert(in_animation); }
	}
	for (auto const& material : ret.materials) {
		auto asset = asset::Material{};
		auto json = data_source().read_json(material);
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
