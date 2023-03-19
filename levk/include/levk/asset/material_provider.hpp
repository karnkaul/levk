#pragma once
#include <levk/asset/asset_provider.hpp>
#include <levk/graphics/material.hpp>
#include <levk/serializer.hpp>

namespace levk {
class TextureProvider;

class MaterialProvider : public GraphicsAssetProvider<UMaterial> {
  public:
	MaterialProvider(TextureProvider& texture_provider, Serializer const& serializer);

	TextureProvider& texture_provider() const { return *m_texture_provider; }
	Serializer const& serializer() const { return *m_serializer; }

  private:
	Payload load_payload(Uri<UMaterial> const& uri) const override;

	Ptr<TextureProvider> m_texture_provider{};
	Ptr<Serializer const> m_serializer{};
};
} // namespace levk
