#pragma once
#include <levk/asset/asset_provider.hpp>
#include <levk/graphics/skeleton.hpp>

namespace levk {
class SkeletonProvider : public AssetProvider<Skeleton> {
  public:
	using AssetProvider<Skeleton>::AssetProvider;

  private:
	Payload load_payload(Uri<Skeleton> const& uri, Stopwatch const& stopwatch) const override;
};
} // namespace levk
