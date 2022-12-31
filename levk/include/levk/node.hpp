#pragma once
#include <levk/transform.hpp>
#include <levk/util/id.hpp>
#include <levk/util/ptr.hpp>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace levk {
class Entity;

struct NodeCreateInfo;

class Node {
  public:
	using CreateInfo = NodeCreateInfo;
	class Tree;

	Id<Node> id() const { return m_id; }
	Id<Node> parent() const { return m_parent; }
	std::span<Id<Node> const> children() const { return m_children; }

	Id<Entity> entity{};
	Transform transform{};
	std::string name{};

  private:
	Id<Node> m_id{};
	Id<Node> m_parent{};
	std::vector<Id<Node>> m_children{};
};

struct NodeCreateInfo {
	Transform transform{};
	std::string name{};
	Id<Node> parent{};
	Id<Entity> entity{};
};

class Node::Tree {
  public:
	using Map = std::unordered_map<Id<Node>::id_type, Node>;

	Node& add(CreateInfo const& create_info);
	void remove(Id<Node> id);

	Ptr<Node const> find(Id<Node> id) const;
	Ptr<Node> find(Id<Node> id);

	Node const& get(Id<Node> id) const;
	Node& get(Id<Node> id);

	void reparent(Node& out, Id<Node> new_parent);
	glm::mat4 global_transform(Node const& node) const;

	std::span<Id<Node> const> roots() const { return m_roots; }

	Map const& map() const { return m_nodes; }

  private:
	void set_parent_on_children(Node& out, Id<Node> parent);
	void remove_child_from_parent(Node& out);

	Map m_nodes{};
	std::vector<Id<Node>> m_roots{};
	Id<Node>::id_type m_prev_id{};
};
} // namespace levk
