#pragma once
#include <levk/asset/asset_provider.hpp>
#include <levk/graphics/material.hpp>
#include <levk/io/serializer.hpp>

namespace levk {
class TextureProvider;

class MaterialProvider : public GraphicsAssetProvider<UMaterial> {
  public:
	MaterialProvider(NotNull<TextureProvider*> texture_provider, NotNull<Serializer const*> serializer);

	TextureProvider& texture_provider() const { return *m_texture_provider; }
	Serializer const& serializer() const { return *m_serializer; }

  private:
	Payload load_payload(Uri<UMaterial> const& uri, Stopwatch const& stopwatch) const override;

	NotNull<TextureProvider*> m_texture_provider;
	NotNull<Serializer const*> m_serializer;
};
} // namespace levk
