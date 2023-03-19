#include <levk/font/ascii_font.hpp>
#include <levk/graphics/render_device.hpp>
#include <levk/graphics/text_primitive.hpp>

namespace levk {
TextPrimitive::TextPrimitive(RenderDevice& render_device) : primitive(render_device.make_dynamic()) {}

void TextPrimitive::update(std::string_view text, AsciiFont& font) {
	if (!primitive) { return; }
	auto geometry = Geometry{};
	auto pen = AsciiFont::Pen{font, height};
	pen.write_line(text, {&geometry, &material.textures.uris[0]});
	primitive->set_geometry(geometry);
}
} // namespace levk
