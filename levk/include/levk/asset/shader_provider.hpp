#pragma once
#include <levk/asset/asset_provider.hpp>
#include <levk/graphics/shader_code.hpp>

namespace levk {
class ShaderProvider : public AssetProvider<ShaderCode> {
  public:
	ShaderProvider(NotNull<DataSource const*> data_source) : AssetProvider<ShaderCode>(data_source, "ShaderProvider") {}

  private:
	Payload load_payload(Uri<ShaderCode> const& uri, Stopwatch const& stopwatch) const override;
};
} // namespace levk
