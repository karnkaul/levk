#include <levk/graphics/draw_list.hpp>
#include <levk/graphics/render_device.hpp>
#include <levk/ui/primitive.hpp>

namespace levk::ui {
Primitive::Primitive(RenderDevice const& render_device) : m_primitive(render_device.make_dynamic()) { m_material.render_mode.depth_test = false; }

void Primitive::set_quad(QuadCreateInfo const& create_info) {
	assert(m_primitive);
	m_primitive->set_geometry(make_quad(create_info));
}

void Primitive::render(DrawList& out) const {
	if (!m_primitive) { return; }
	auto const rot = glm::angleAxis(glm::radians(z_rotation), front_v);
	auto const mat = glm::translate(glm::toMat4(rot), {world_frame().centre(), z_index});
	out.add(m_primitive.get(), &m_material, DrawList::Instanced{.parent = mat});
	Node::render(out);
}
} // namespace levk::ui
