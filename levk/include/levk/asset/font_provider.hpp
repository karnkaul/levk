#pragma once
#include <levk/asset/asset_provider.hpp>
#include <levk/font/ascii_font.hpp>

namespace levk {
class TextureProvider;
class FontLibrary;

class AsciiFontProvider : public GraphicsAssetProvider<AsciiFont> {
  public:
	AsciiFontProvider(TextureProvider& texture_provider, FontLibrary const& font_library);

	TextureProvider& texture_provider() const { return *m_texture_provider; }
	FontLibrary const& font_library() const { return *m_font_library; }

  private:
	Payload load_payload(Uri<AsciiFont> const& uri) const override;

	Ptr<TextureProvider> m_texture_provider{};
	Ptr<FontLibrary const> m_font_library{};
};
} // namespace levk
