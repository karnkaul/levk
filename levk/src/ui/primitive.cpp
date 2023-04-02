#include <levk/graphics/draw_list.hpp>
#include <levk/graphics/render_device.hpp>
#include <levk/ui/primitive.hpp>

namespace levk::ui {
Primitive::Primitive(RenderDevice const& render_device) : m_primitive(render_device.vulkan_device()) { m_material.render_mode.depth_test = false; }

void Primitive::render(DrawList& out) const {
	auto const rot = glm::angleAxis(glm::radians(z_rotation), front_v);
	auto const mat = glm::translate(glm::toMat4(rot), {world_frame().centre(), z_index});
	out.add(&m_primitive, &m_material, DrawList::Instances{.parent = mat});
	View::render(out);
}
} // namespace levk::ui
