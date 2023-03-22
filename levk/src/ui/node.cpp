#include <levk/ui/node.hpp>
#include <algorithm>

namespace levk::ui {
glm::vec2 Node::world_position() const { return m_super_node ? m_super_node->world_position() + position : position; }

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
