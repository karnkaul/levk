#pragma once
#include <levk/node/node.hpp>
#include <levk/util/ptr.hpp>
#include <unordered_map>

namespace levk {
class NodeTree {
  public:
	using CreateInfo = NodeCreateInfo;
	using Map = std::unordered_map<Id<Node>::id_type, Node>;

	// Defined in node_tree_serializer
	struct Serializer;

	Node& add(CreateInfo const& create_info);
	void remove(Id<Node> id);

	Ptr<Node const> find(Id<Node> id) const;
	Ptr<Node> find(Id<Node> id);

	Node const& get(Id<Node> id) const;
	Node& get(Id<Node> id);

	void reparent(Node& out, Id<Node> new_parent);
	glm::mat4 global_transform(Node const& node) const;
	glm::mat4 global_transform(Id<Node> id) const;

	Id<Node> find_by_name(std::string_view name) const;

	void clear();

	std::span<Id<Node> const> roots() const { return m_roots; }

	Map const& map() const { return m_nodes; }

	template <typename Func>
	void for_each(Func&& func) {
		for (auto& [_, node] : m_nodes) { func(node); }
	}

  private:
	static Node make_node(Id<Node> self, std::vector<Id<Node>> children, CreateInfo create_info = {});

	void remove_child_from_parent(Node& out);
	void destroy_children(Node& out);

	Map m_nodes{};
	std::vector<Id<Node>> m_roots{};
	Id<Node>::id_type m_prev_id{};
};

///
/// \brief Enables updating nodes but not adding/removing nodes to/from the tree.
///
/// Animations / editors / etc use NodeLocators to affect nodes safely without
/// any possibility of adding/removing nodes to/from the tree.
///
class NodeLocator {
  public:
	NodeLocator(NodeTree& out_tree) : m_out(out_tree) {}

	Ptr<Node> find(Id<Node> id) const { return m_out.find(id); }
	Node& get(Id<Node> id) const { return m_out.get(id); }
	void reparent(Node& out, Id<Node> new_parent) const { return m_out.reparent(out, new_parent); }
	glm::mat4 global_transform(Node const& node) const { return m_out.global_transform(node); }
	std::span<Id<Node> const> roots() const { return m_out.roots(); }
	NodeTree::Map const& map() const { return m_out.map(); }

  private:
	NodeTree& m_out;
};
} // namespace levk
