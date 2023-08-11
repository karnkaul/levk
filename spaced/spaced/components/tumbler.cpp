#include <levk/scene/entity.hpp>
#include <spaced/components/tumbler.hpp>

namespace spaced {
void Tumbler::tick(levk::Duration dt) {
	auto const drot = glm::slerp(glm::identity<glm::quat>(), rot_per_s, dt.count());
	owning_entity()->transform().set_orientation(owning_entity()->transform().orientation() * drot);
}
} // namespace spaced
