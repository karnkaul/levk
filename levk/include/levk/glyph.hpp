#pragma once
#include <levk/pixel_map.hpp>

namespace levk {
enum struct Codepoint : std::uint32_t {
	eNull = 0u,
};

enum struct TextHeight : std::uint32_t {
	eDefault = 32u,
};

struct Glyph {
	struct Provider;

	Bitmap<ByteArray> pixmap{};
	glm::ivec2 left_top{};
	Codepoint codepoint{};
};

struct Glyph::Provider {
	struct Null;

	virtual ~Provider() = default;

	virtual bool set_height(TextHeight height) = 0;
	virtual TextHeight height() const = 0;
	virtual Glyph glyph_for(Codepoint codepoint) const = 0;
};

struct Glyph::Provider::Null : Glyph::Provider {
	using Provider::Provider;

	bool set_height(TextHeight) final { return true; }
	TextHeight height() const final { return {}; }
	Glyph glyph_for(Codepoint) const final { return {}; }
};
} // namespace levk
