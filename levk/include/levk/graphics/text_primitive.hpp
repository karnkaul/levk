#pragma once
#include <levk/font/common.hpp>
#include <levk/graphics/material.hpp>
#include <levk/graphics/primitive.hpp>

namespace levk {
class AsciiFont;
class RenderDevice;

class TextPrimitive {
  public:
	TextPrimitive(RenderDevice& render_device);

	void update(std::string_view text, AsciiFont& font);

	std::unique_ptr<Primitive::Dynamic> primitive{};
	UnlitMaterial material{};
	TextHeight height{TextHeight::eDefault};
};
} // namespace levk
