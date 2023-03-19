#include <levk/asset/common.hpp>
#include <levk/asset/material_provider.hpp>
#include <levk/asset/mesh_provider.hpp>
#include <levk/graphics/render_device.hpp>

namespace levk {
SkeletonProvider::Payload SkeletonProvider::load_payload(Uri<Skeleton> const& uri) const {
	auto ret = Payload{};
	auto json = data_source().read_json(uri);
	if (!json) {
		logger::error("[SkeletonProvider] Failed to load JSON [{}]", uri.value());
		return {};
	}
	if (json["asset_type"].as_string() != "skeleton") {
		logger::error("[SkeletonProvider] JSON is not a Skeleton [{}]", uri.value());
		return {};
	}
	auto asset = asset::Skeleton{};
	asset::from_json(json, asset);
	ret.asset.emplace();
	ret.asset->name = std::string{json["name"].as_string()};
	ret.asset->joints = std::move(asset.joints);
	for (auto const& in_animation : asset.animations) {
		auto const in_animation_asset = data_source().read(in_animation);
		if (!in_animation_asset) { continue; }
		auto out_animation_asset = asset::BinSkeletalAnimation{};
		if (!out_animation_asset.read(read_bytes(in_animation.value()).span())) {
			logger::warn("[Load] Failed to load SkeletalAnimation at: [{}]", in_animation.value());
			continue;
		}
		ret.dependencies.push_back(in_animation);
		auto source = Skeleton::Animation::Source{
			.target_joints = std::move(out_animation_asset.target_joints),
			.name = std::move(out_animation_asset.name),
		};
		for (auto const& in_sampler : out_animation_asset.samplers) { source.animation.samplers.push_back({in_sampler}); }
		ret.asset->animation_sources.push_back(std::move(source));
	}
	ret.asset->self = uri;
	ret.dependencies.push_back(uri);
	logger::info("[SkeletonProvider] Skeleton loaded [{}]", uri.value());
	return ret;
}

StaticMeshProvider::Payload StaticMeshProvider::load_payload(Uri<StaticMesh> const& uri) const {
	auto ret = Payload{};
	auto json = data_source().read_json(uri);
	if (!json) {
		logger::error("[StaticMeshProvider] Failed to load JSON [{}]", uri.value());
		return {};
	}
	if (json["type"].as_string() != "static") {
		logger::error("[StaticMeshProvider] JSON is not a StaticMesh [{}]", uri.value());
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
		auto primitive = render_device().make_static(bin_geometry.geometry);
		if (in_primitive.material) { material_provider().load(in_primitive.material); }
		ret.asset->primitives.push_back({std::move(primitive), in_primitive.material});
	}
	ret.dependencies.push_back(uri);
	logger::info("[StaticMeshProvider] StaticMesh loaded [{}]", uri.value());
	return ret;
}

SkinnedMeshProvider::SkinnedMeshProvider(SkeletonProvider& skeleton_provider, MaterialProvider& material_provider)
	: MeshProviderCommon<SkinnedMesh>(material_provider), m_skeleton_provider(&skeleton_provider) {}

SkinnedMeshProvider::Payload SkinnedMeshProvider::load_payload(Uri<SkinnedMesh> const& uri) const {
	auto ret = Payload{};
	auto json = data_source().read_json(uri);
	if (!json) {
		logger::error("[SkinnedMeshProvider] Failed to load JSON [{}]", uri.value());
		return {};
	}
	if (json["type"].as_string() != "skinned") {
		logger::error("[SkinnedMeshProvider] JSON is not a SkinnedMesh [{}]", uri.value());
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
		auto geometry = render_device().make_skinned(bin_geometry.geometry, {bin_geometry.joints, bin_geometry.weights});
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
