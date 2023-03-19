#pragma once
#include <levk/defines.hpp>
#include <levk/font/font_library.hpp>
#include <levk/util/unique.hpp>

static_assert(levk::freetype_v);

#include <ft2build.h>
#include FT_FREETYPE_H

namespace levk {
struct FreetypeDeleter {
	void operator()(FT_Face face) const {
		if (face != FT_Face{}) { FT_Done_Face(face); }
	}
};

class FreetypeGlyphFactory : public GlyphSlot::Factory {
  public:
	FreetypeGlyphFactory(FT_Face face, ByteArray bytes);

	bool set_height(TextHeight height) final;
	TextHeight height() const final { return m_height; }
	GlyphSlot slot_for(Codepoint codepoint) const final;

  private:
	Unique<FT_Face, FreetypeDeleter> m_face{};
	ByteArray m_bytes{};
	TextHeight m_height{};
};

class Freetype : public FontLibrary {
  public:
	bool init() final;
	std::unique_ptr<GlyphSlot::Factory> load(ByteArray bytes) const final;

  private:
	FT_Library m_lib{};
};
} // namespace levk
