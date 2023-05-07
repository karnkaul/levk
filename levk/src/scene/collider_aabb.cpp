#include <levk/level/attachments.hpp>
#include <levk/scene/collider_aabb.hpp>
#include <levk/scene/scene.hpp>

namespace levk {
Aabb ColliderAabb::get_aabb() const {
	if (auto* collision = scene_collision()) { return collision->get(m_on_collision.id()); }
	return {};
}

void ColliderAabb::set_aabb(Aabb const& aabb) {
	if (auto* collision = scene_collision()) { collision->set(m_on_collision.id(), aabb); }
}

void ColliderAabb::setup() {
	if (auto* collision = scene_collision()) { m_on_collision = collision->add({}); }
}

void ColliderAabb::tick(Duration) {
	auto* entity = owning_entity();
	auto* scene = owning_scene();
	auto* collision = scene_collision();
	if (!entity || !scene || !collision) { return; }

	auto aabb = get_aabb();
	aabb.origin = Transform::from(scene->global_transform(*entity)).position();
	set_aabb(aabb);
	if (auto other = collision->colliding_with(m_on_collision.id())) { on_collision(other); }
}

std::unique_ptr<Attachment> ColliderAabb::to_attachment() const {
	auto ret = std::make_unique<ColliderAttachment>();
	ret->aabb = get_aabb();
	return ret;
}
} // namespace levk
