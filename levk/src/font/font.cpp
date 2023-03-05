#include <levk/font/font.hpp>
#include <levk/util/zip_ranges.hpp>

namespace levk {
Font::Font(std::unique_ptr<GlyphSlot::Factory> slot_factory, TextureProvider& texture_provider, Uri<Texture> uri_prefix)
	: m_slot_factory(std::move(slot_factory)), m_uri_prefix(std::move(uri_prefix)), m_texure_provider(&texture_provider) {}

Font::Glyph const& Font::glyph_for(Ascii const codepoint, TextHeight const height) {
	static constexpr auto null_v{Glyph{}};
	if (!m_slot_factory) { return null_v; }
	auto* typeface = try_get_typeface(clamp(height));
	if (!typeface) { return null_v; }
	auto const* ret = typeface->try_get_glyph(*m_slot_factory, codepoint);
	if (!ret) { return null_v; }
	return *ret;
}

std::size_t Font::make_glyphs(std::span<Ascii const> codepoints, TextHeight const height) {
	if (!m_slot_factory) { return {}; }
	auto* typeface = try_get_typeface(clamp(height));
	if (!typeface) { return {}; }
	auto ret = std::size_t{};
	for (auto const codepoint : codepoints) {
		if (typeface->try_get_glyph(*m_slot_factory, codepoint)) { ++ret; }
	}
	return ret;
}

Ptr<TextureAtlas const> Font::texture_atlas(TextHeight const height) const {
	if (auto it = m_typefaces.find(clamp(height)); it != m_typefaces.end()) { return &it->second.atlas; }
	return {};
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

Ptr<Font::Glyph const> Font::Typeface::try_get_glyph(GlyphSlot::Factory& slot_factory, Ascii const codepoint) {
	auto it = entries.find(codepoint);
	if (it == entries.end()) {
		if (!slot_factory.set_height(height)) { return {}; }
		auto slot = slot_factory.slot_for(static_cast<Codepoint>(codepoint), height);
		if (!slot) { return {}; }
		glm::ivec2 const advance = {slot.advance.x >> 6, slot.advance.y >> 6};
		auto glyph = Glyph{.advance = advance, .left_top = slot.left_top};
		if (slot.has_pixmap()) {
			auto pixels = DynPixelMap{slot.pixmap.extent};
			for (auto [rgba, pixel] : zip_ranges(pixels.span(), slot.pixmap.storage.span())) {
				rgba.channels.x = rgba.channels.y = rgba.channels.z = rgba.channels.w = static_cast<std::uint8_t>(pixel);
			}
			glyph.pix_rect = TextureAtlas::Writer{atlas}.write(pixels.view());
			glyph.extent = slot.pixmap.extent;
		}
		auto [i, _] = entries.insert_or_assign(codepoint, std::move(glyph));
		it = i;
	}
	return &it->second;
}
} // namespace levk
