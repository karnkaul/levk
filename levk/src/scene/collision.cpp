#include <levk/scene/collision.hpp>
#include <levk/scene/scene.hpp>

namespace levk {
Aabb ColliderAabb::aabb() const {
	auto ret = Aabb{.size = aabb_size};
	if (auto* entity = owning_entity()) { ret.origin = entity->global_position(); }
	return ret;
}

void ColliderAabb::setup() {
	assert(owning_entity());
	owning_scene()->collision.track(*owning_entity());
}

void Collision::track(Entity const& entity) {
	if (!entity.contains<ColliderAabb>()) { return; }
	m_entries.emplace(entity.id(), Entry{});
}

void Collision::tick(Scene const& scene, Duration dt) {
	auto colliders = std::vector<Ptr<Entry>>{};
	for (auto it = m_entries.begin(); it != m_entries.end();) {
		auto* entity = scene.find_entity(it->first);
		if (!entity) {
			it = m_entries.erase(it);
			continue;
		}
		auto& entry = it->second;
		entry.collider = entity->find_component<ColliderAabb>();
		if (!entry.collider) {
			it = m_entries.erase(it);
			continue;
		}
		entry.active = entity->is_active;
		if (!entry.active) {
			++it;
			continue;
		}
		entry.colliding = false;
		entry.aabb = entry.collider->aabb();
		colliders.push_back(&entry);
		++it;
	}
	if (colliders.empty()) { return; }

	auto integrate = [&](Entry& out_a, Entry& out_b) {
		if (Aabb::intersects(out_a.aabb, out_b.aabb)) { return true; }
		if (!out_a.previous_position) { return false; }
		if (out_a.aabb.origin != *out_a.previous_position) {
			for (auto t = Duration{}; t < dt; t += time_slice) {
				auto const position_a = glm::mix(*out_a.previous_position, out_a.aabb.origin, t / dt);
				if (Aabb::intersects(Aabb{position_a, out_a.aabb.size}, out_b.aabb)) { return true; }
			}
		}
		return false;
	};
	for (auto it_a = colliders.begin(); it_a + 1 != colliders.end(); ++it_a) {
		auto& a = **it_a;
		if (!a.active) { continue; }
		for (auto it_b = it_a + 1; it_b != colliders.end(); ++it_b) {
			auto& b = **it_b;
			if (!b.active) { continue; }
			auto const ignore = a.collider->ignore_channels && b.collider->ignore_channels && (a.collider->ignore_channels & b.collider->ignore_channels);
			if (ignore) { continue; }
			if (integrate(a, b)) {
				if (a.collider->on_collision) { a.collider->on_collision(*b.collider); }
				if (b.collider->on_collision) { b.collider->on_collision(*a.collider); }
				a.colliding = b.colliding = true;
			}
		}
		a.previous_position = a.aabb.origin;
	}
}

void Collision::clear() { m_entries.clear(); }
} // namespace levk
