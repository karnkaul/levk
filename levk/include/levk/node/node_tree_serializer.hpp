#pragma once
#include <djson/json.hpp>
#include <levk/node/node_tree.hpp>

namespace levk {
struct NodeTree::Serializer {
	static void serialize(dj::Json& out, NodeTree const& tree);
	static void deserialize(dj::Json const& json, NodeTree& out);
};
} // namespace levk
