#include <levk/asset/common.hpp>
#include <levk/asset/skeleton_provider.hpp>

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
			logger::warn("[SkeletonProvider] Failed to load SkeletalAnimation at: [{}]", in_animation.value());
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
} // namespace levk
