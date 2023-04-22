#pragma once
#include <levk/transform.hpp>
#include <levk/util/id.hpp>
#include <span>
#include <string>
#include <vector>

namespace levk {
class Entity;

struct NodeCreateInfo;

class Node {
  public:
	using CreateInfo = NodeCreateInfo;
	class Tree;
	class Locator;

	Id<Node> id() const { return m_id; }
	Id<Node> parent() const { return m_parent; }
	std::span<Id<Node> const> children() const { return m_children; }

	Id<Entity> entity_id{};
	Transform transform{};
	std::string name{};

  private:
	Id<Node> m_id{};
	Id<Node> m_parent{};
	std::vector<Id<Node>> m_children{};

	friend class NodeTree;
};

struct NodeCreateInfo {
	Transform transform{};
	std::string name{};
	Id<Node> parent{};
	Id<Entity> entity_id{};
};
} // namespace levk
