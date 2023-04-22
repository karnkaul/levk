#include <levk/asset/asset_list.hpp>
#include <levk/asset/asset_providers.hpp>
#include <levk/asset/mesh_provider.hpp>
#include <levk/io/common.hpp>
#include <levk/level/attachments.hpp>
#include <levk/scene/collider_aabb.hpp>
#include <levk/scene/entity.hpp>
#include <levk/scene/freecam_controller.hpp>
#include <levk/scene/scene.hpp>
#include <levk/scene/shape_renderer.hpp>
#include <levk/scene/skeleton_controller.hpp>
#include <levk/scene/skinned_mesh_renderer.hpp>
#include <levk/scene/static_mesh_renderer.hpp>
#include <levk/service.hpp>

namespace levk {
bool ShapeAttachment::serialize(dj::Json& out) const {
	out["shape"] = shape;
	if (!instances.empty()) {
		auto& out_instances = out["instances"];
		for (auto const& transform : instances) { to_json(out_instances.push_back({}), transform); }
	}
	return true;
}

bool ShapeAttachment::deserialize(dj::Json const& json) {
	shape = json["shape"];
	for (auto const& instance : json["instances"].array_view()) { from_json(instance, instances.emplace_back()); }
	return true;
}

void ShapeAttachment::attach(Entity& out) {
	auto* serializer = Service<Serializer>::find();
	if (!shape || !serializer) { return; }
	auto result = serializer->deserialize_as<Shape>(shape);
	if (!result) { return; }
	auto& renderer = out.attach(std::make_unique<ShapeRenderer>());
	renderer.set_shape(std::move(result.value));
}

void ShapeAttachment::add_assets(AssetList& out) const {
	if (material_uri) { out.materials.insert(material_uri); }
}

bool MeshAttachment::serialize(dj::Json& out) const {
	out["uri"] = uri.value();
	if (!instances.empty()) {
		auto& out_instances = out["instances"];
		for (auto const& transform : instances) { to_json(out_instances.push_back({}), transform); }
	}
	return true;
}

bool MeshAttachment::deserialize(dj::Json const& json) {
	uri = json["uri"].as<std::string>();
	for (auto const& instance : json["instances"].array_view()) { from_json(instance, instances.emplace_back()); }
	return true;
}

void MeshAttachment::attach(Entity& out) {
	auto* asset_providers = Service<AssetProviders>::find();
	if (!asset_providers) { return; }
	switch (asset_providers->mesh_type(uri)) {
	case MeshType::eSkinned: {
		asset_providers->skinned_mesh().load(uri);
		auto& smr = out.attach(std::make_unique<SkinnedMeshRenderer>());
		smr.set_mesh_uri(uri);
		smr.instances = std::move(instances);
		break;
	}
	case MeshType::eStatic: {
		asset_providers->static_mesh().load(uri);
		auto& smr = out.attach(std::make_unique<StaticMeshRenderer>());
		smr.mesh_uri = uri;
		smr.instances = std::move(instances);
		break;
	}
	default: break;
	}
}

void MeshAttachment::add_assets(AssetList& out) const { out.meshes.insert(uri); }

bool SkeletonAttachment::serialize(dj::Json& out) const {
	if (enabled_index) { out["enabled_index"] = *enabled_index; }
	return true;
}

bool SkeletonAttachment::deserialize(dj::Json const& json) {
	if (auto& index = json["enabled_index"]) { enabled_index = index.as<std::size_t>(); }
	return true;
}

void SkeletonAttachment::attach(Entity& out) {
	auto& controller = out.attach(std::make_unique<SkeletonController>());
	if (enabled_index) { controller.enabled = *enabled_index; }
}

bool FreecamAttachment::serialize(dj::Json& out) const {
	to_json(out["move_speed"], move_speed);
	out["look_speed"] = look_speed;
	out["pitch"] = pitch;
	out["yaw"] = yaw;
	return true;
}

bool FreecamAttachment::deserialize(dj::Json const& json) {
	from_json(json["move_speed"], move_speed);
	look_speed = json["look_speed"].as<float>(look_speed);
	pitch = json["pitch"].as<float>(pitch);
	yaw = json["yaw"].as<float>(yaw);
	return true;
}

void FreecamAttachment::attach(Entity& out) {
	out.attach(std::make_unique<FreecamController>());
	if (auto* scene = out.owning_scene()) { scene->camera.target = out.id(); }
}

bool ColliderAttachment::serialize(dj::Json& out) const {
	to_json(out["aabb"], aabb);
	return true;
}

bool ColliderAttachment::deserialize(dj::Json const& json) {
	from_json(json["aabb"], aabb);
	return true;
}

void ColliderAttachment::attach(Entity& out) {
	auto& collider = out.attach(std::make_unique<ColliderAabb>());
	collider.set_aabb(aabb);
}
} // namespace levk
