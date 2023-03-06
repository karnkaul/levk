#pragma once
#include <levk/font/font_glyph.hpp>
#include <levk/font/glyph_slot.hpp>
#include <levk/texture_atlas.hpp>
#include <optional>

namespace levk {
class StaticFontAtlas {
  public:
	struct CreateInfo {
		GlyphSlot::Factory& slot_factory;
		TextureProvider& texture_provider;
		Uri<Texture> texture_uri;

		std::span<Codepoint const> codepoints{};
		TextHeight height{TextHeight::eDefault};
	};

	StaticFontAtlas(CreateInfo const& create_info);

	TextHeight height() const { return m_height; }
	Uri<Texture> texture_uri() const { return m_atlas ? m_atlas->texture_uri() : Uri<Texture>{}; }
	bool contains(Codepoint const codepoint) const { return m_glyphs.contains(codepoint); }
	Ptr<FontGlyph const> glyph_for(Codepoint codepoint) const;

	explicit operator bool() const { return !m_glyphs.empty(); }

  private:
	std::optional<TextureAtlas> m_atlas{};
	std::unordered_map<Codepoint, FontGlyph> m_glyphs{};
	TextHeight m_height{};
};
} // namespace levk
