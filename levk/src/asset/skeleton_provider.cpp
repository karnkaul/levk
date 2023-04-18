#include <levk/asset/asset_io.hpp>
#include <levk/asset/skeleton_provider.hpp>

namespace levk {
SkeletalAnimationProvider::Payload SkeletalAnimationProvider::load_payload(Uri<SkeletalAnimation> const& uri, Stopwatch const& stopwatch) const {
	auto const in_animation_asset = data_source().read(uri);
	if (!in_animation_asset) { return {}; }
	auto out_animation_asset = asset::BinSkeletalAnimation{};
	if (!out_animation_asset.read(read_bytes(uri.value()).span())) {
		m_logger.warn("Failed to load SkeletalAnimation at: [{}]", uri.value());
		return {};
	}
	auto ret = Payload{};
	ret.dependencies.push_back(uri);
	ret.asset.emplace();
	ret.asset->name = std::move(out_animation_asset.name);
	ret.asset->target_joints.reserve(out_animation_asset.target_joints.size());
	for (auto const& joint : out_animation_asset.target_joints) { ret.asset->target_joints.push_back(joint); }
	for (auto const& in_sampler : out_animation_asset.samplers) { ret.asset->animation.samplers.push_back({in_sampler}); }
	m_logger.info("[{:.3f}s] SkeletalAnimation loaded [{}]", stopwatch().count(), uri.value());
	return ret;
}

SkeletonProvider::Payload SkeletonProvider::load_payload(Uri<Skeleton> const& uri, Stopwatch const& stopwatch) const {
	auto ret = Payload{};
	auto json = data_source().read_json(uri);
	if (!json) {
		m_logger.error("Failed to load JSON [{}]", uri.value());
		return {};
	}
	ret.asset.emplace();
	asset::from_json(json, *ret.asset);
	for (auto const& in_animation : ret.asset->animations) { animation_provider().load(in_animation); }
	ret.dependencies.push_back(uri);
	m_logger.info("[{:.3f}s] Skeleton loaded [{}]", stopwatch().count(), uri.value());
	return ret;
}
} // namespace levk
