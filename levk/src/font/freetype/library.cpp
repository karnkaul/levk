#include <font/freetype/library.hpp>

namespace levk {
FreetypeGlyphFactory::FreetypeGlyphFactory(FT_Face face, ByteArray bytes) : m_face(face), m_bytes(std::move(bytes)) {}

bool FreetypeGlyphFactory::set_height(TextHeight height) {
	if (FT_Set_Pixel_Sizes(m_face, 0, static_cast<FT_UInt>(height)) == FT_Err_Ok) {
		m_height = height;
		return true;
	}
	return false;
}

GlyphSlot FreetypeGlyphFactory::slot_for(Codepoint codepoint) const {
	if (!m_face.get()) { return {}; }
	if (FT_Load_Char(m_face, static_cast<FT_ULong>(codepoint), FT_LOAD_RENDER) != FT_Err_Ok) { return {}; }
	if (!m_face->glyph) { return {}; }
	auto ret = GlyphSlot{.codepoint = codepoint};
	auto const& glyph = *m_face->glyph;
	ret.pixmap.channels = 1;
	ret.pixmap.extent = {glyph.bitmap.width, glyph.bitmap.rows};
	auto const size = static_cast<std::size_t>(ret.pixmap.extent.x * ret.pixmap.extent.y);
	if (size > 0) {
		ret.pixmap.storage = ByteArray{size};
		std::memcpy(ret.pixmap.storage.data(), glyph.bitmap.buffer, ret.pixmap.storage.size());
	}
	ret.left_top = {glyph.bitmap_left, glyph.bitmap_top};
	ret.advance = {static_cast<std::int32_t>(m_face->glyph->advance.x), static_cast<std::int32_t>(m_face->glyph->advance.y)};
	return ret;
}

bool Freetype::init() { return FT_Init_FreeType(&m_lib) == FT_Err_Ok; }

std::unique_ptr<GlyphSlot::Factory> Freetype::load(ByteArray bytes) const {
	if (!m_lib) { return {}; }
	auto face = FT_Face{};
	if (FT_New_Memory_Face(m_lib, reinterpret_cast<FT_Byte const*>(bytes.data()), static_cast<FT_Long>(bytes.size()), 0, &face) != FT_Err_Ok) { return {}; }
	return std::make_unique<FreetypeGlyphFactory>(face, std::move(bytes));
}
} // namespace levk
