#include <levk/font/ascii_font.hpp>
#include <levk/util/zip_ranges.hpp>

namespace levk {
AsciiFont::AsciiFont(std::unique_ptr<GlyphSlot::Factory> slot_factory, TextureProvider& texture_provider, Uri<Texture> uri_prefix)
	: m_slot_factory(std::move(slot_factory)), m_uri_prefix(std::move(uri_prefix)), m_texure_provider(&texture_provider) {}

FontGlyph const& AsciiFont::glyph_for(Ascii const codepoint, TextHeight height) {
	height = clamp(height);
	if (auto* atlas = make_font_atlas(height)) {
		if (auto* ret = atlas->glyph_for(static_cast<Codepoint>(codepoint))) { return *ret; }
	}
	static constexpr auto null_v{FontGlyph{}};
	return null_v;
}

Ptr<StaticFontAtlas const> AsciiFont::make_font_atlas(TextHeight height) {
	if (auto* ret = find_font_atlas(height)) { return ret; }
	if (!m_slot_factory) { return {}; }
	auto uri = fmt::format("{}.{}", m_uri_prefix.value(), static_cast<int>(height));
	auto sfaci = StaticFontAtlas::CreateInfo{
		.slot_factory = *m_slot_factory,
		.texture_provider = *m_texure_provider,
		.texture_uri = std::move(uri),
		.height = height,
	};
	if (auto atlas = StaticFontAtlas{sfaci}) {
		auto [it, _] = m_atlases.insert_or_assign(height, std::move(atlas));
		return &it->second;
	}
	return {};
}

Ptr<StaticFontAtlas const> AsciiFont::find_font_atlas(TextHeight const height) const {
	if (auto it = m_atlases.find(clamp(height)); it != m_atlases.end()) { return &it->second; }
	return {};
}

void AsciiFont::destroy_font_atlas(TextHeight height) { m_atlases.erase(height); }

Uri<Texture> AsciiFont::texture_uri(TextHeight height) const {
	height = clamp(height);
	if (m_atlases.contains(height)) { return fmt::format("{}.{}", m_uri_prefix.value(), static_cast<int>(height)); }
	return {};
}
} // namespace levk
