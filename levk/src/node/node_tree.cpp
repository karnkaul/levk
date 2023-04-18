#include <levk/asset/asset_io.hpp>
#include <levk/node/node_tree.hpp>
#include <levk/util/error.hpp>
#include <levk/util/logger.hpp>
#include <algorithm>
#include <utility>

namespace levk {
namespace {
auto const g_log{Logger{"NodeTree"}};
}

Node& NodeTree::add(CreateInfo const& create_info) {
	auto node = Node{};
	node.m_id = ++m_prev_id;
	node.transform = create_info.transform;
	node.entity_id = create_info.entity_id;
	assert(node.m_id != create_info.parent);
	node.name = create_info.name.empty() ? "(Unnamed)" : create_info.name;
	if (create_info.parent) {
		if (auto it = m_nodes.find(create_info.parent); it != m_nodes.end()) {
			it->second.m_children.push_back(node.m_id);
			node.m_parent = create_info.parent;
		} else {
			g_log.warn("Invalid parent Id<Node>: {}", create_info.parent.value());
		}
	}
	if (!node.m_parent) { m_roots.push_back(node.m_id); }
	auto [it, _] = m_nodes.insert_or_assign(node.m_id, std::move(node));
	return it->second;
}

void NodeTree::remove(Id<Node> id) {
	if (auto it = m_nodes.find(id); it != m_nodes.end()) {
		remove_child_from_parent(it->second);
		destroy_children(it->second);
		m_nodes.erase(it);
		std::erase(m_roots, id);
	}
}

void NodeTree::reparent(Node& out, Id<Node> new_parent) {
	assert(out.m_id != new_parent);
	if (!out.m_parent && new_parent) { std::erase(m_roots, out.m_id); }
	if (out.m_parent && !new_parent) { m_roots.push_back(out.m_id); }
	remove_child_from_parent(out);
	if (auto* parent = find(new_parent)) { parent->m_children.push_back(out.m_id); }
	out.m_parent = new_parent;
}

glm::mat4 NodeTree::global_transform(Node const& node) const {
	auto ret = node.transform.matrix();
	if (auto const* parent = find(node.parent())) { return global_transform(*parent) * ret; }
	return ret;
}

void NodeTree::clear() {
	m_nodes.clear();
	m_roots.clear();
}

Id<Node> NodeTree::find_by_name(std::string_view name) const {
	if (name.empty()) { return {}; }
	for (auto const& [id, node] : m_nodes) {
		if (node.name == name) { return id; }
	}
	return {};
}

Node NodeTree::make_node(Id<Node> self, std::vector<Id<Node>> children, CreateInfo create_info) {
	auto ret = Node{};
	ret.m_id = self;
	ret.m_parent = create_info.parent;
	ret.m_children = std::move(children);
	ret.entity_id = create_info.entity_id;
	ret.transform = create_info.transform;
	ret.name = std::move(create_info.name);
	return ret;
}

void NodeTree::remove_child_from_parent(Node& out) {
	if (auto* parent = find(out.m_parent)) {
		std::erase(parent->m_children, out.m_id);
		out.m_parent = {};
	}
}

void NodeTree::destroy_children(Node& out) {
	for (auto const id : out.m_children) {
		if (auto it = m_nodes.find(id); it != m_nodes.end()) {
			destroy_children(it->second);
			m_nodes.erase(it);
		}
	}
	out.m_children.clear();
}

Ptr<Node const> NodeTree::find(Id<Node> id) const {
	if (id == Id<Node>{}) { return {}; }
	if (auto it = m_nodes.find(id); it != m_nodes.end()) { return &it->second; }
	return {};
}

Ptr<Node> NodeTree::find(Id<Node> id) { return const_cast<Node*>(std::as_const(*this).find(id)); }

Node const& NodeTree::get(Id<Node> id) const {
	auto const* ret = find(id);
	if (!ret) { throw Error{fmt::format("Invalid entity id: {}", id)}; }
	return *ret;
}

Node& NodeTree::get(Id<Node> id) { return const_cast<Node&>(std::as_const(*this).get(id)); }
} // namespace levk
