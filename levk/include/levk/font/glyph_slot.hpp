#pragma once
#include <levk/font/common.hpp>
#include <levk/graphics/pixel_map.hpp>

namespace levk {
struct GlyphSlot {
	struct Factory;

	Bitmap<ByteArray> pixmap{};
	glm::ivec2 left_top{};
	glm::ivec2 advance{};
	Codepoint codepoint{};

	bool has_pixmap() const { return !pixmap.storage.empty(); }

	explicit operator bool() const { return advance.x > 0 || advance.y > 0; }
};

struct GlyphSlot::Factory {
	struct Null;

	virtual ~Factory() = default;

	virtual bool set_height(TextHeight height) = 0;
	virtual TextHeight height() const = 0;
	virtual GlyphSlot slot_for(Codepoint codepoint) const = 0;

	GlyphSlot slot_for(Codepoint codepoint, TextHeight height) {
		if (!set_height(height)) { return {}; }
		return slot_for(codepoint);
	}
};

struct GlyphSlot::Factory::Null : GlyphSlot::Factory {
	using Factory::Factory;

	bool set_height(TextHeight) final { return true; }
	TextHeight height() const final { return {}; }
	GlyphSlot slot_for(Codepoint) const final { return {}; }
};
} // namespace levk
