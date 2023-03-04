#include <levk/font.hpp>
#include <levk/util/zip_ranges.hpp>

namespace levk {
Font::Font(std::unique_ptr<GlyphSlot::Factory> slot_factory, TextureProvider& texture_provider, Uri<Texture> uri_prefix)
	: m_slot_factory(std::move(slot_factory)), m_uri_prefix(std::move(uri_prefix)), m_texure_provider(&texture_provider) {}

Font::Glyph Font::glyph_for(Codepoint const codepoint, TextHeight const height) {
	if (!m_slot_factory) { return {}; }
	auto* typeface = try_get_typeface(clamp(height));
	if (!typeface) { return {}; }
	auto const* entry = typeface->try_get_entry(*m_slot_factory, codepoint);
	if (!entry) { return {}; }
	auto ret = entry->glyph;
	if (entry->cell) { ret.uv_rect = typeface->atlas.uv_rect_for(*entry->cell); }
	return ret;
}

Uri<Texture> Font::texture_uri(TextHeight const height) const {
	if (m_typefaces.contains(clamp(height))) { return fmt::format("{}.{}", m_uri_prefix.value(), static_cast<int>(height)); }
	return {};
}

Ptr<Font::Typeface> Font::try_get_typeface(TextHeight const height) {
	if (auto it = m_typefaces.find(height); it != m_typefaces.end()) { return &it->second; }
	if (!m_slot_factory->set_height(height)) { return {}; }
	auto uri = fmt::format("{}.{}", m_uri_prefix.value(), static_cast<int>(height));
	auto atlas = TextureAtlas{*m_texure_provider, std::move(uri), {.initial_extent = m_atlas_extent}};
	auto typeface = Typeface{.atlas = std::move(atlas), .height = height};
	auto [it, _] = m_typefaces.insert_or_assign(height, std::move(typeface));
	return &it->second;
}

Ptr<Font::Entry const> Font::Typeface::try_get_entry(GlyphSlot::Factory& slot_factory, Codepoint const codepoint) {
	auto it = entries.find(codepoint);
	if (it == entries.end()) {
		auto slot = slot_factory.slot_for(codepoint, height);
		if (!slot) { return {}; }
		auto entry = Entry{.glyph = Glyph{.advance = slot.advance, .left_top = slot.left_top}};
		if (slot.has_pixmap()) {
			auto pixels = DynPixelMap{slot.pixmap.extent};
			for (auto [rgba, pixel] : zip_ranges(pixels.span(), slot.pixmap.storage.span())) {
				rgba.channels.x = rgba.channels.y = rgba.channels.z = rgba.channels.w = static_cast<std::uint8_t>(pixel);
			}
			entry.cell = atlas.inscribe(pixels.view());
		}
		auto [i, _] = entries.insert_or_assign(codepoint, std::move(entry));
		it = i;
	}
	return &it->second;
}
} // namespace levk
