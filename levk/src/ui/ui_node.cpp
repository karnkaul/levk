#include <levk/ui/ui_node.hpp>
#include <algorithm>

namespace levk {
glm::vec2 UINode::world_position() const { return m_super_node ? m_super_node->world_position() + position : position; }

Ptr<UINode> UINode::add_sub_node(std::unique_ptr<UINode> node) {
	if (!node) { return {}; }
	node->m_super_node = this;
	return m_sub_nodes.emplace_back(std::move(node)).get();
}

void UINode::tick(Input const& input, Time dt) {
	if (is_destroyed()) { return; }
	for (auto const& node : m_sub_nodes) { node->tick(input, dt); }
	std::erase_if(m_sub_nodes, [](auto const& node) { return node->is_destroyed(); });
}

void UINode::render(DrawList& out) const {
	for (auto const& view : m_sub_nodes) { view->render(out); }
}
} // namespace levk
