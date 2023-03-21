#pragma once
#include <levk/asset/asset_provider.hpp>
#include <levk/font/ascii_font.hpp>

namespace levk {
class TextureProvider;
struct FontLibrary;

class AsciiFontProvider : public GraphicsAssetProvider<AsciiFont> {
  public:
	AsciiFontProvider(NotNull<TextureProvider*> texture_provider, NotNull<FontLibrary const*> font_library);

	TextureProvider& texture_provider() const { return *m_texture_provider; }
	FontLibrary const& font_library() const { return *m_font_library; }

  private:
	Payload load_payload(Uri<AsciiFont> const& uri) const override;

	NotNull<TextureProvider*> m_texture_provider;
	NotNull<FontLibrary const*> m_font_library;
};
} // namespace levk
