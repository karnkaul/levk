#include <levk/io/common.hpp>
#include <levk/node/node_tree_serializer.hpp>

namespace levk {
void NodeTree::Serializer::serialize(dj::Json& out, NodeTree const& tree) {
	auto& out_nodes = out["nodes"];
	for (auto const& [id, in_node] : tree.m_nodes) {
		auto out_node = dj::Json{};
		out_node["name"] = in_node.name;
		to_json(out_node["transform"], in_node.transform);
		out_node["id"] = in_node.id().value();
		out_node["parent"] = in_node.parent().value();
		if (!in_node.children().empty()) {
			auto out_children = dj::Json{};
			for (auto id : in_node.children()) { out_children.push_back(id.value()); }
			out_node["children"] = std::move(out_children);
		}
		out_nodes.push_back(std::move(out_node));
	}
	auto& out_roots = out["roots"];
	for (auto const id : tree.m_roots) { out_roots.push_back(id.value()); }
	out["max_id"] = tree.m_prev_id;
}

void NodeTree::Serializer::deserialize(dj::Json const& json, NodeTree& out) {
	for (auto const& in_node : json["nodes"].array_view()) {
		auto transform = Transform{};
		from_json(in_node["transform"], transform);
		auto nci = Node::CreateInfo{
			.transform = transform,
			.name = in_node["name"].as<std::string>(),
			.parent = in_node["parent"].as<std::size_t>(),
		};
		auto children = std::vector<Id<Node>>{};
		for (auto const& in_child : in_node["children"].array_view()) { children.push_back(in_child.as<Id<Node>::id_type>()); }
		auto const id = in_node["id"].as<std::size_t>();
		out.m_nodes.insert_or_assign(id, make_node(id, std::move(children), std::move(nci)));
		out.m_prev_id = std::max(out.m_prev_id, id);
	}
	for (auto const& in_root : json["roots"].array_view()) { out.m_roots.push_back(in_root.as<std::size_t>()); }
	out.m_prev_id = std::max(out.m_prev_id, json["max_id"].as<Id<Node>::id_type>());
}
} // namespace levk
