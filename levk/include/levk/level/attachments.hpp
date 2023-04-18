#pragma once
#include <levk/level/attachment.hpp>
#include <levk/transform.hpp>
#include <levk/uri.hpp>
#include <levk/util/id.hpp>
#include <optional>

namespace levk {
class Material;
struct Mesh;
class Entity;
struct AssetList;

struct ShapeAttachment : Attachment {
	dj::Json shape{};
	Uri<Material> material_uri{};
	std::vector<Transform> instances{};

	std::string_view type_name() const override { return "ShapeAttachment"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;
	void attach(Entity& out) override;
	void add_assets(AssetList& out) const override;
};

struct MeshAttachment : Attachment {
	Uri<Mesh> uri{};
	std::vector<Transform> instances{};

	std::string_view type_name() const override { return "MeshAttachment"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;
	void attach(Entity& out) override;
	void add_assets(AssetList& out) const override;
};

struct SkeletonAttachment : Attachment {
	std::optional<std::size_t> enabled_index{};

	std::string_view type_name() const override { return "SkeletonAttachment"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;
	void attach(Entity& out) override;
};

struct FreecamAttachment : Attachment {
	glm::vec3 move_speed{10.0f};
	float look_speed{0.3f};
	float pitch{};
	float yaw{};

	std::string_view type_name() const override { return "FreecamAttachment"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;
	void attach(Entity& out) override;
};
} // namespace levk
