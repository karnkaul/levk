#include <levk/scene/scene.hpp>
#include <spaced/actors/asteroid.hpp>
#include <spaced/components/health_bar.hpp>
#include <spaced/components/hit_target.hpp>

namespace spaced {
void Asteroid::tick(levk::Duration) {
	owning_scene()->get_component<HealthBar>(health_bar).n_value = owning_entity()->get_component<HitTarget>().hp.normalized();
}
} // namespace spaced
