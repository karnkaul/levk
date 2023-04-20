#include <levk/collision.hpp>
#include <utility>

namespace levk {
Collision::Scoped::Scoped(Scoped&& rhs) noexcept : m_handle(std::exchange(rhs.m_handle, std::nullopt)) {}

Collision::Scoped& Collision::Scoped::operator=(Scoped&& rhs) noexcept {
	if (&rhs != this) {
		disconnect();
		m_handle = std::exchange(rhs.m_handle, std::nullopt);
	}
	return *this;
}

void Collision::Scoped::disconnect() {
	if (m_handle) {
		m_handle->collision->remove(m_handle->id);
		m_handle.reset();
	}
}

auto Collision::add(Aabb aabb) -> Handle {
	auto [id, ret] = m_entries.add(Entry{aabb});
	return Handle{id, this};
}

void Collision::remove(Id<Aabb> id) { m_entries.remove(id); }

Aabb Collision::get(Id<Aabb> id) const {
	if (auto* ret = m_entries.find(id)) { return ret->aabb; }
	return {};
}

void Collision::set(Id<Aabb> id, Aabb aabb) {
	if (auto* ret = m_entries.find(id)) { ret->aabb = aabb; }
}

Id<Aabb> Collision::colliding_with(Id<Aabb> id) const {
	auto* entry = m_entries.find(id);
	return entry ? entry->colliding_with : Id<Aabb>{};
}

void Collision::update() {
	for (auto& [_, entry] : m_entries) { entry.colliding_with = {}; }

	for (auto& [id_a, entry_a] : m_entries) {
		for (auto& [id_b, entry_b] : m_entries) {
			if (id_a == id_b) { continue; }
			if (Aabb::intersects(entry_a.aabb, entry_b.aabb)) {
				entry_a.colliding_with = id_b;
				entry_b.colliding_with = id_a;
			}
		}
	}
}
} // namespace levk
