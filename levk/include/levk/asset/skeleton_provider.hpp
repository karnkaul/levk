#pragma once
#include <levk/asset/asset_provider.hpp>
#include <levk/graphics/skeleton.hpp>

namespace levk {
class SkeletonProvider : public AssetProvider<Skeleton> {
  public:
	SkeletonProvider(NotNull<DataSource const*> data_source) : AssetProvider<Skeleton>(data_source, "SkeletonProvider") {}

  private:
	Payload load_payload(Uri<Skeleton> const& uri, Stopwatch const& stopwatch) const override;
};
} // namespace levk
