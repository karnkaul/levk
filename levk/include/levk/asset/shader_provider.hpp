#pragma once
#include <levk/asset/asset_provider.hpp>
#include <levk/graphics_common.hpp>

namespace levk {
class ShaderProvider : public AssetProvider<ShaderCode> {
  public:
	using AssetProvider<ShaderCode>::AssetProvider;

  private:
	Payload load_payload(Uri<ShaderCode> const& uri) const override;
};
} // namespace levk
