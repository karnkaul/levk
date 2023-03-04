#pragma once
#include <levk/glyph_slot.hpp>
#include <levk/texture_atlas.hpp>
#include <optional>

namespace levk {
class Font {
  public:
	struct Glyph {
		glm::vec2 advance{};
		std::optional<UvRect> uv_rect{};
		glm::vec2 left_top{};

		explicit constexpr operator bool() const { return advance.x > 0 || advance.y > 0; }
	};

	Font(std::unique_ptr<GlyphSlot::Factory> slot_factory, TextureProvider& texture_provider, Uri<Texture> uri_prefix);

	Glyph glyph_for(Codepoint codepoint, TextHeight height = TextHeight::eDefault);
	Uri<Texture> texture_uri(TextHeight height = TextHeight::eDefault) const;

	explicit operator bool() const { return m_slot_factory != nullptr; }

  private:
	struct Entry {
		std::optional<TextureAtlas::Cell> cell{};
		Glyph glyph{};
	};
	struct Typeface {
		TextureAtlas atlas;
		std::unordered_map<Codepoint, Entry> entries{};
		TextHeight height{};

		Ptr<Entry const> try_get_entry(GlyphSlot::Factory& slot_factory, Codepoint codepoint);
	};

	Ptr<Typeface> try_get_typeface(TextHeight height);

	std::unique_ptr<GlyphSlot::Factory> m_slot_factory{};
	std::unordered_map<TextHeight, Typeface> m_typefaces{};
	Uri<Texture> m_uri_prefix{};
	Extent2D m_atlas_extent{512u, 512u};
	Ptr<TextureProvider> m_texure_provider{};
};
} // namespace levk
