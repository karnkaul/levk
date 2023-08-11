#include <levk/scene/entity.hpp>
#include <spaced/components/displacer.hpp>

namespace spaced {
void Displacer::tick(levk::Duration dt) {
	auto* entity = owning_entity();
	assert(entity);
	if (entity->is_active) { entity->transform().set_position(entity->transform().position() + velocity * dt.count()); }
}
} // namespace spaced
