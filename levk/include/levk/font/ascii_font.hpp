#pragma once
#include <levk/font/static_font_atlas.hpp>

namespace levk {
enum struct Ascii : char {};

class AsciiFont {
  public:
	AsciiFont(std::unique_ptr<GlyphSlot::Factory> slot_factory, TextureProvider& texture_provider, Uri<Texture> uri_prefix);

	FontGlyph const& glyph_for(Ascii codepoint, TextHeight height = TextHeight::eDefault);

	Ptr<StaticFontAtlas const> make_font_atlas(TextHeight height);
	Ptr<StaticFontAtlas const> find_font_atlas(TextHeight height) const;
	void destroy_font_atlas(TextHeight height);

	Uri<Texture> texture_uri(TextHeight height = TextHeight::eDefault) const;

	explicit operator bool() const { return m_slot_factory != nullptr; }

  private:
	std::unordered_map<TextHeight, StaticFontAtlas> m_atlases{};
	std::unique_ptr<GlyphSlot::Factory> m_slot_factory{};
	Uri<Texture> m_uri_prefix{};
	Ptr<TextureProvider> m_texure_provider{};
};
} // namespace levk
