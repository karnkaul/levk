#pragma once
#include <capo/pcm.hpp>
#include <levk/asset/asset_provider.hpp>

namespace levk {
class PcmProvider : public AssetProvider<capo::Pcm> {
  public:
	PcmProvider(NotNull<DataSource const*> data_source) : AssetProvider<capo::Pcm>(data_source, "PcmProvider") {}

  private:
	Payload load_payload(Uri<capo::Pcm> const& uri, Stopwatch const& stopwatch) const override;
};
} // namespace levk
