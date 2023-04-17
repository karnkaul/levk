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
	struct Attachment {
		Uri<Mesh> mesh_uri{};
		dj::Json shape{};
		std::vector<Transform> mesh_instances{};
		std::vector<Transform> shape_instances{};
		std::optional<std::size_t> enabled_animation{};

		explicit operator bool() const { return mesh_uri || shape; }
	};

	Camera camera{};
	Lights lights{};
	NodeTree node_tree{};
	std::unordered_map<Id<Node>::id_type, Attachment> attachments{};
	std::string name{};
};
} // namespace levk
