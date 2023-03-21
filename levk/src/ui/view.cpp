#include <levk/graphics/draw_list.hpp>
#include <levk/ui/view.hpp>
#include <levk/window/window_state.hpp>
#include <algorithm>

namespace levk::ui {
View::View() noexcept { m_material.render_mode.depth_test = false; }

glm::vec2 View::global_position() const { return m_super_view ? m_super_view->global_position() + position : position; }

Ptr<View> View::add_sub_view(std::unique_ptr<View> view) {
	if (!view) { return {}; }
	view->m_super_view = this;
	return m_sub_views.emplace_back(std::move(view)).get();
}

void View::tick(Input const& input, Time dt) {
	if (is_destroyed()) { return; }
	for (auto const& view : m_sub_views) { view->tick(input, dt); }
	std::erase_if(m_sub_views, [](auto const& v) { return v->is_destroyed(); });
}

void View::render(DrawList& out) const {
	for (auto const& view : m_sub_views) { view->render(out); }
}

void View::draw(Primitive::Dynamic& primitive, DrawList& out) const {
	auto const rot = glm::angleAxis(glm::radians(z_rotation), front_v);
	auto pos = global_position();
	auto const mat = glm::translate(glm::toMat4(rot), {pos, z_index});
	out.add(&primitive, &m_material, DrawList::Instanced{.parent = mat});
}
} // namespace levk::ui
