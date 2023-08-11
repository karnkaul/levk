#include <spaced/components/hit_target.hpp>

namespace spaced {
bool HitTarget::take_damage(float damage) {
	hp.modify(-damage);
	if (hp.value <= 0.0f) { return true; }
	return false;
}
} // namespace spaced
