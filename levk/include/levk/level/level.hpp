#pragma once
#include <djson/json.hpp>
#include <levk/graphics/camera.hpp>
#include <levk/graphics/lights.hpp>
#include <levk/node/node_tree.hpp>
#include <levk/uri.hpp>
#include <optional>

namespace levk {
struct Mesh;

struct Level {
	Camera camera{};
	Lights lights{};
	NodeTree node_tree{};
	std::unordered_map<Id<Node>::id_type, std::vector<dj::Json>> attachments_map{};
	std::string name{};
};
} // namespace levk
