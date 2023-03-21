#pragma once
#include <levk/font/static_font_atlas.hpp>
#include <levk/util/not_null.hpp>

namespace levk {
enum struct Ascii : char {
	eTofu = '\0',
};

class AsciiFont {
  public:
	class Pen;
	struct Out;

	AsciiFont(std::unique_ptr<GlyphSlot::Factory> slot_factory, NotNull<TextureProvider*> texture_provider, Uri<Texture> uri_prefix);

	FontGlyph const& glyph_for(Ascii ascii, TextHeight height);

	Ptr<StaticFontAtlas const> make_font_atlas(TextHeight height);
	Ptr<StaticFontAtlas const> find_font_atlas(TextHeight height) const;
	void destroy_font_atlas(TextHeight height);

	Uri<Texture> texture_uri(TextHeight height = TextHeight::eDefault) const;
	TextureProvider& texture_provider() const { return *m_texure_provider; }

	explicit operator bool() const { return m_slot_factory != nullptr; }

  private:
	std::unordered_map<TextHeight, StaticFontAtlas> m_atlases{};
	std::unique_ptr<GlyphSlot::Factory> m_slot_factory{};
	Uri<Texture> m_uri_prefix{};
	NotNull<TextureProvider*> m_texure_provider;
};

struct AsciiFont::Out {
	Ptr<Geometry> geometry{};
	Ptr<Uri<Texture>> atlas{};
};

class AsciiFont::Pen {
  public:
	Pen(AsciiFont& font, TextHeight height = TextHeight::eDefault) : m_font(font), m_height(clamp(height)) {}

	Pen& write_line(std::string_view line, Out out);

	TextHeight height() const { return m_height; }

	glm::vec3 cursor{};
	Rgba vertex_colour{white_v};

  private:
	AsciiFont& m_font;
	TextHeight m_height;
};
} // namespace levk
