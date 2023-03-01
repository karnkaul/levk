#pragma once
#include <levk/glyph.hpp>
#include <memory>

namespace levk {
struct FontLibrary {
	struct Null;

	virtual ~FontLibrary() = default;

	virtual bool init() = 0;
	virtual std::unique_ptr<Glyph::Provider> load(ByteArray bytes) const = 0;
};

struct FontLibrary::Null : FontLibrary {
	bool init() final { return true; }
	std::unique_ptr<Glyph::Provider> load(ByteArray) const final { return std::make_unique<Glyph::Provider::Null>(); }
};
} // namespace levk
