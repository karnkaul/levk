#pragma once
#include <levk/font/glyph_slot.hpp>
#include <memory>

namespace levk {
struct FontLibrary {
	struct Null;

	virtual ~FontLibrary() = default;

	virtual bool init() = 0;
	virtual std::unique_ptr<GlyphSlot::Factory> load(ByteArray bytes) const = 0;
};

struct FontLibrary::Null : FontLibrary {
	bool init() final { return true; }
	std::unique_ptr<GlyphSlot::Factory> load(ByteArray) const final { return std::make_unique<GlyphSlot::Factory::Null>(); }
};
} // namespace levk
