#include <levk/ui/view.hpp>
#include <algorithm>

namespace levk::ui {
Rect View::world_frame() const {
	if (!m_super_view) { return m_frame; }
	auto const super_frame = m_super_view->world_frame();
	auto const origin = super_frame.centre() + n_anchor * super_frame.extent();
	return Rect::from_extent(m_frame.extent(), origin + m_frame.centre());
}

void View::set_frame(Rect frame) {
	auto const extent = frame.extent();
	if (extent.x < 0.0f || extent.y < 0.0f) { return; }
	m_frame = frame;
}

void View::set_position(glm::vec2 position) { m_frame = Rect::from_extent(m_frame.extent(), position); }

void View::set_extent(glm::vec2 extent) { set_frame(Rect::from_extent(extent, m_frame.centre())); }

Ptr<View> View::add_sub_view(std::unique_ptr<View> view) {
	if (!view) { return {}; }
	view->m_super_view = this;
	return m_sub_views.emplace_back(std::move(view)).get();
}

bool View::contains(Ptr<View const> view) const {
	for (auto const& sub_view : m_sub_views) {
		if (sub_view.get() == view || sub_view->contains(view)) { return true; }
	}
	return false;
}

void View::tick(WindowInput const& window_input, Duration dt) {
	if (is_destroyed()) { return; }
	for (auto const& view : m_sub_views) { view->tick(window_input, dt); }
	std::erase_if(m_sub_views, [](auto const& view) { return view->is_destroyed(); });
}

void View::render(DrawList& out) const {
	for (auto const& view : m_sub_views) { view->render(out); }
}
} // namespace levk::ui
