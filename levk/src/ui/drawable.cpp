#include <levk/geometry.hpp>
#include <levk/graphics/render_device.hpp>
#include <levk/ui/drawable.hpp>

namespace levk::ui {
Drawable::Drawable(RenderDevice const& render_device) : primitive(render_device.make_dynamic()) {}

void Drawable::render(DrawList& out) const {
	if (primitive) { draw(*primitive, out); }
	View::render(out);
}

void Drawable::set_quad(QuadCreateInfo const& create_info) {
	if (!primitive) { return; }
	primitive->set_geometry(make_quad(create_info.size, create_info.rgb, create_info.origin, create_info.uv));
}
} // namespace levk::ui
