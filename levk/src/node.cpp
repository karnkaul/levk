#include <levk/node.hpp>
#include <levk/util/error.hpp>
#include <levk/util/logger.hpp>
#include <algorithm>
#include <utility>

namespace levk {
Node& Node::Tree::add(CreateInfo const& create_info) {
	auto node = Node{};
	node.m_id = ++m_next_id;
	node.transform = create_info.transform;
	node.entity = create_info.entity;
	assert(node.m_id != create_info.parent);
	auto const* name = create_info.name.empty() ? "(Unnamed)" : create_info.name.c_str();
	node.name = fmt::format("{} ({})", name, node.m_id.value());
	if (create_info.parent) {
		if (auto it = m_nodes.find(create_info.parent); it != m_nodes.end()) {
			it->second.m_children.push_back(node.m_id);
			node.m_parent = create_info.parent;
		} else {
			logger::warn("[Tree] Invalid parent Id<Node>: {}", create_info.parent.value());
		}
	}
	if (!create_info.parent) { m_roots.push_back(node.m_id); }
	auto [it, _] = m_nodes.insert_or_assign(node.m_id, std::move(node));
	return it->second;
}

void Node::Tree::remove(Id<Node> id) {
	if (auto it = m_nodes.find(id); it != m_nodes.end()) {
		set_parent_on_children(it->second, {});
		remove_child_from_parent(it->second);
		m_nodes.erase(it);
		std::erase(m_roots, id);
	}
}

void Node::Tree::reparent(Node& out, Id<Node> new_parent) {
	assert(out.m_id != new_parent);
	if (!out.m_parent && new_parent) { std::erase(m_roots, out.m_id); }
	if (out.m_parent && !new_parent) { m_roots.push_back(out.m_id); }
	set_parent_on_children(out, new_parent);
	remove_child_from_parent(out);
	if (auto* parent = find(new_parent)) { parent->m_children.push_back(out.m_id); }
	out.m_parent = new_parent;
}

glm::mat4 Node::Tree::global_transform(Node const& node) const {
	auto ret = node.transform.matrix();
	if (auto const* parent = find(node.parent())) { return global_transform(*parent) * ret; }
	return ret;
}

void Node::Tree::set_parent_on_children(Node& out, Id<Node> parent) {
	for (auto& child : out.m_children) {
		if (auto* entity = find(child)) { entity->m_parent = parent; }
	}
}

void Node::Tree::remove_child_from_parent(Node& out) {
	if (auto* parent = find(out.m_parent)) { std::erase(parent->m_children, out.m_id); }
}

Ptr<Node const> Node::Tree::find(Id<Node> id) const {
	if (id == Id<Node>{}) { return {}; }
	if (auto it = m_nodes.find(id); it != m_nodes.end()) { return &it->second; }
	return {};
}

Ptr<Node> Node::Tree::find(Id<Node> id) { return const_cast<Node*>(std::as_const(*this).find(id)); }

Node const& Node::Tree::get(Id<Node> id) const {
	auto const* ret = find(id);
	if (!ret) { throw Error{fmt::format("Invalid entity id: {}", id)}; }
	return *ret;
}

Node& Node::Tree::get(Id<Node> id) { return const_cast<Node&>(std::as_const(*this).get(id)); }
} // namespace levk
