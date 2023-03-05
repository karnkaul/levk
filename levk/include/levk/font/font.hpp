#pragma once
#include <levk/font/glyph_slot.hpp>
#include <levk/texture_atlas.hpp>
#include <optional>

namespace levk {
enum struct Ascii : char {};

class Font {
  public:
	using PixRect = TextureAtlas::Cell;

	struct Glyph {
		glm::vec2 advance{};
		glm::vec2 extent{};
		std::optional<PixRect> pix_rect{};
		glm::vec2 left_top{};

		constexpr Rect2D<> rect(glm::vec2 baseline) const { return {.lt = baseline + left_top, .rb = baseline + left_top + glm::vec2{extent.x, -extent.y}}; }

		explicit constexpr operator bool() const { return advance.x > 0 || advance.y > 0; }
	};

	Font(std::unique_ptr<GlyphSlot::Factory> slot_factory, TextureProvider& texture_provider, Uri<Texture> uri_prefix);

	Glyph const& glyph_for(Ascii codepoint, TextHeight height = TextHeight::eDefault);
	std::size_t make_glyphs(std::span<Ascii const> codepoints, TextHeight height);

	Ptr<TextureAtlas const> texture_atlas(TextHeight height = TextHeight::eDefault) const;
	Uri<Texture> texture_uri(TextHeight height = TextHeight::eDefault) const;

	explicit operator bool() const { return m_slot_factory != nullptr; }

  private:
	struct Typeface {
		TextureAtlas atlas;
		std::unordered_map<Ascii, Glyph> entries{};
		TextHeight height{};

		Ptr<Glyph const> try_get_glyph(GlyphSlot::Factory& slot_factory, Ascii codepoint);
	};

	Ptr<Typeface> try_get_typeface(TextHeight height);

	std::unique_ptr<GlyphSlot::Factory> m_slot_factory{};
	std::unordered_map<TextHeight, Typeface> m_typefaces{};
	Uri<Texture> m_uri_prefix{};
	Extent2D m_atlas_extent{512u, 512u};
	Ptr<TextureProvider> m_texure_provider{};
};
} // namespace levk
