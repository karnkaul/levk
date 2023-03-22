#include <levk/asset/texture_provider.hpp>
#include <levk/font/ascii_font.hpp>
#include <levk/ui/text.hpp>

namespace levk::ui {
Text::Text(NotNull<AsciiFont*> font) : Primitive(font->texture_provider().render_device()), m_font(font) {}

void Text::set_string(std::string string) {
	if (string != m_string) {
		m_string = std::move(string);
		refresh();
	}
}

void Text::set_height(TextHeight height) {
	if (height = clamp(height); height != m_height) {
		m_height = height;
		refresh();
	}
}

void Text::refresh() {
	auto geometry = Geometry{};
	auto pen = AsciiFont::Pen{*m_font, m_height};
	auto const extent = pen.line_extent(m_string);
	pen.cursor = {(n_anchor - 0.5f) * extent, z_index};
	pen.write_line(m_string, {&geometry, &texture_uri()});
	m_primitive->set_geometry(geometry);
}
} // namespace levk::ui
