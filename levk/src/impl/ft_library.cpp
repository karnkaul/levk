#include <impl/ft_library.hpp>

namespace levk {
FtGlyphProvider::FtGlyphProvider(FT_Face face, ByteArray bytes) : m_face(face), m_bytes(std::move(bytes)) {}

bool FtGlyphProvider::set_height(TextHeight height) {
	if (FT_Set_Pixel_Sizes(m_face, 0, static_cast<FT_UInt>(height)) == FT_Err_Ok) {
		m_height = height;
		return true;
	}
	return false;
}

Glyph FtGlyphProvider::glyph_for(Codepoint codepoint) const {
	if (!m_face.get()) { return {}; }
	if (FT_Load_Char(m_face, static_cast<FT_ULong>(codepoint), FT_LOAD_RENDER) != FT_Err_Ok) { return {}; }
	if (!m_face->glyph) { return {}; }
	auto ret = Glyph{.codepoint = codepoint};
	auto const& bitmap = m_face->glyph->bitmap;
	ret.pixmap.channels = 1;
	ret.pixmap.extent = {bitmap.width, bitmap.rows};
	if (ret.pixmap.extent.x == 0 || ret.pixmap.extent.y == 0) { return ret; }
	ret.pixmap.storage = ByteArray{static_cast<std::size_t>(ret.pixmap.extent.x * ret.pixmap.extent.y)};
	std::memcpy(ret.pixmap.storage.data(), bitmap.buffer, ret.pixmap.storage.size());
	return ret;
}

bool FtLib::init() { return FT_Init_FreeType(&m_lib) == FT_Err_Ok; }

std::unique_ptr<Glyph::Provider> FtLib::load(ByteArray bytes) const {
	if (!m_lib) { return {}; }
	auto face = FT_Face{};
	if (FT_New_Memory_Face(m_lib, reinterpret_cast<FT_Byte const*>(bytes.data()), static_cast<FT_Long>(bytes.size()), 0, &face) != FT_Err_Ok) { return {}; }
	return std::make_unique<FtGlyphProvider>(face, std::move(bytes));
}
} // namespace levk
