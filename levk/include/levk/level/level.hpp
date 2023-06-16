#pragma once
#include <djson/json.hpp>
#include <levk/graphics/camera.hpp>
#include <levk/graphics/lights.hpp>
#include <levk/node/node_tree.hpp>
#include <levk/uri.hpp>
#include <optional>

namespace levk {
class Cubemap;

struct Level {
	Camera camera{};
	Lights lights{};
	Uri<Cubemap> skybox{};
	NodeTree node_tree{};
	std::unordered_map<Id<Node>::id_type, std::vector<dj::Json>> attachments_map{};
	std::string name{};
};
} // namespace levk
