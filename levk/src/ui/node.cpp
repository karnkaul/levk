#include <levk/ui/node.hpp>
#include <algorithm>

namespace levk::ui {
Rect Node::world_frame() const {
	if (!m_super_node) { return m_frame; }
	auto const super_frame = m_super_node->world_frame();
	auto const origin = super_frame.centre() + n_anchor * super_frame.extent();
	return Rect::from_extent(m_frame.extent(), origin + m_frame.centre());
}

void Node::set_frame(Rect frame) {
	auto const extent = frame.extent();
	if (extent.x < 0.0f || extent.y < 0.0f) { return; }
	m_frame = frame;
}

void Node::set_position(glm::vec2 position) { m_frame = Rect::from_extent(m_frame.extent(), position); }

void Node::set_extent(glm::vec2 extent) { set_frame(Rect::from_extent(extent, m_frame.centre())); }

Ptr<Node> Node::add_sub_node(std::unique_ptr<Node> node) {
	if (!node) { return {}; }
	node->m_super_node = this;
	return m_sub_nodes.emplace_back(std::move(node)).get();
}

void Node::tick(Input const& input, Time dt) {
	if (is_destroyed()) { return; }
	for (auto const& node : m_sub_nodes) { node->tick(input, dt); }
	std::erase_if(m_sub_nodes, [](auto const& node) { return node->is_destroyed(); });
}

void Node::render(DrawList& out) const {
	for (auto const& view : m_sub_nodes) { view->render(out); }
}
} // namespace levk::ui
