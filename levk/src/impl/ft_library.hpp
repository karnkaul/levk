#pragma once
#include <levk/defines.hpp>
#include <levk/font_library.hpp>
#include <levk/util/unique.hpp>

static_assert(levk::freetype_v);

#include <ft2build.h>
#include FT_FREETYPE_H

namespace levk {
struct FtDeleter {
	void operator()(FT_Face face) const {
		if (face != FT_Face{}) { FT_Done_Face(face); }
	}
};

class FtGlyphProvider : public Glyph::Provider {
  public:
	FtGlyphProvider(FT_Face face, ByteArray bytes);

	bool set_height(TextHeight height) final;
	TextHeight height() const final { return m_height; }
	Glyph glyph_for(Codepoint codepoint) const final;

  private:
	Unique<FT_Face, FtDeleter> m_face{};
	ByteArray m_bytes{};
	TextHeight m_height{};
};

class FtLib : public FontLibrary {
  public:
	bool init() final;
	std::unique_ptr<Glyph::Provider> load(ByteArray bytes) const final;

  private:
	FT_Library m_lib{};
};
} // namespace levk
