#pragma once
#include <levk/aabb.hpp>
#include <levk/level/attachment.hpp>
#include <levk/transform.hpp>
#include <levk/uri.hpp>
#include <levk/util/id.hpp>
#include <optional>
#include <vector>

namespace levk {
class Material;
struct Mesh;
class Entity;
struct AssetList;

struct ShapeAttachment : Attachment {
	dj::Json shape{};
	Uri<Material> material_uri{};
	std::vector<Transform> instances{};

	std::string_view type_name() const final { return "ShapeAttachment"; }
	bool serialize(dj::Json& out) const final;
	bool deserialize(dj::Json const& json) final;
	void attach(Entity& out) final;
	void add_assets(AssetList& out) const final;
};

struct MeshAttachment : Attachment {
	Uri<Mesh> uri{};
	std::vector<Transform> instances{};

	std::string_view type_name() const final { return "MeshAttachment"; }
	bool serialize(dj::Json& out) const final;
	bool deserialize(dj::Json const& json) final;
	void attach(Entity& out) final;
	void add_assets(AssetList& out) const final;
};

struct SkeletonAttachment : Attachment {
	std::optional<std::size_t> enabled_index{};

	std::string_view type_name() const final { return "SkeletonAttachment"; }
	bool serialize(dj::Json& out) const final;
	bool deserialize(dj::Json const& json) final;
	void attach(Entity& out) final;
};

struct FreecamAttachment : Attachment {
	glm::vec3 move_speed{10.0f};
	float look_speed{0.3f};
	float pitch{};
	float yaw{};

	std::string_view type_name() const final { return "FreecamAttachment"; }
	bool serialize(dj::Json& out) const final;
	bool deserialize(dj::Json const& json) final;
	void attach(Entity& out) final;
};

struct ColliderAttachment : Attachment {
	glm::vec3 aabb_size{};

	std::string_view type_name() const final { return "ColliderAttachment"; }
	bool serialize(dj::Json& out) const final;
	bool deserialize(dj::Json const& json) final;
	void attach(Entity& out) final;
};
} // namespace levk
