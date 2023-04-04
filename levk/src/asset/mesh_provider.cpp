#include <levk/asset/common.hpp>
#include <levk/asset/material_provider.hpp>
#include <levk/asset/mesh_provider.hpp>
#include <levk/asset/skeleton_provider.hpp>
#include <levk/graphics/render_device.hpp>

namespace levk {
StaticMeshProvider::Payload StaticMeshProvider::load_payload(Uri<StaticMesh> const& uri) const {
	auto ret = Payload{};
	auto json = data_source().read_json(uri);
	if (!json) {
		logger::error("[StaticMeshProvider] Failed to load JSON [{}]", uri.value());
		return {};
	}
	ret.asset.emplace();
	ret.asset->name = json["name"].as_string();
	auto asset = asset::Mesh3D{};
	asset::from_json(json, asset);
	for (auto const& in_primitive : asset.primitives) {
		assert(!in_primitive.geometry.is_empty());
		auto bin_geometry = asset::BinGeometry{};
		if (!bin_geometry.read(read_bytes(in_primitive.geometry.value()).span())) {
			logger::error("[StaticMeshProvider] Failed to read bin geometry [{}]", in_primitive.geometry.value());
			continue;
		}
		ret.dependencies.push_back(in_primitive.geometry);
		auto primitive = StaticPrimitive{render_device().vulkan_device(), bin_geometry.geometry};
		if (in_primitive.material) { material_provider().load(in_primitive.material); }
		ret.asset->primitives.push_back({std::move(primitive), in_primitive.material});
	}
	ret.dependencies.push_back(uri);
	logger::info("[StaticMeshProvider] StaticMesh loaded [{}]", uri.value());
	return ret;
}

SkinnedMeshProvider::SkinnedMeshProvider(NotNull<SkeletonProvider*> skeleton_provider, NotNull<MaterialProvider*> material_provider)
	: MeshProviderCommon<SkinnedMesh>(material_provider), m_skeleton_provider(skeleton_provider) {}

SkinnedMeshProvider::Payload SkinnedMeshProvider::load_payload(Uri<SkinnedMesh> const& uri) const {
	auto ret = Payload{};
	auto json = data_source().read_json(uri);
	if (!json) {
		logger::error("[SkinnedMeshProvider] Failed to load JSON [{}]", uri.value());
		return {};
	}
	ret.asset.emplace();
	ret.asset->name = json["name"].as_string();
	auto asset = asset::Mesh3D{};
	asset::from_json(json, asset);
	for (auto const& in_primitive : asset.primitives) {
		assert(!in_primitive.geometry.is_empty());
		auto bin_geometry = asset::BinGeometry{};
		if (!bin_geometry.read(read_bytes(in_primitive.geometry.value()).span())) {
			logger::error("[SkinnedMeshProvider] Failed to read bin geometry [{}]", in_primitive.geometry.value());
			continue;
		}
		ret.dependencies.push_back(in_primitive.geometry);
		auto geometry = SkinnedPrimitive{render_device().vulkan_device(), bin_geometry.geometry, {bin_geometry.joints, bin_geometry.weights}};
		if (in_primitive.material) { material_provider().load(in_primitive.material); }
		ret.asset->primitives.push_back({std::move(geometry), in_primitive.material});
	}
	ret.asset->inverse_bind_matrices = asset.inverse_bind_matrices;
	if (auto const& skeleton = json["skeleton"]) {
		auto skeleton_uri = Uri<Skeleton>{skeleton.as<std::string>()};
		if (skeleton_provider().load(skeleton_uri)) { ret.asset->skeleton = std::move(skeleton_uri); }
	}
	ret.dependencies.push_back(uri);
	logger::info("[SkinnedMeshProvider] SkinnedMesh loaded [{}]", uri.value());
	return ret;
}
} // namespace levk
