#pragma once
#include <levk/asset/asset_provider.hpp>
#include <levk/graphics/skeleton.hpp>

namespace levk {
class SkeletalAnimationProvider : public AssetProvider<SkeletalAnimation> {
  public:
	SkeletalAnimationProvider(NotNull<DataSource const*> data_source) : AssetProvider<SkeletalAnimation>(data_source, "SkeletalAnimationProvider") {}

  private:
	Payload load_payload(Uri<SkeletalAnimation> const& uri, Stopwatch const& stopwatch) const override;
};

class SkeletonProvider : public AssetProvider<Skeleton> {
  public:
	SkeletonProvider(NotNull<SkeletalAnimationProvider*> animation_provider, NotNull<DataSource const*> data_source)
		: AssetProvider<Skeleton>(data_source, "SkeletonProvider"), m_animation_provider(animation_provider) {}

	SkeletalAnimationProvider& animation_provider() const { return *m_animation_provider; }

  private:
	Payload load_payload(Uri<Skeleton> const& uri, Stopwatch const& stopwatch) const override;

	Ptr<SkeletalAnimationProvider> m_animation_provider{};
};
} // namespace levk
