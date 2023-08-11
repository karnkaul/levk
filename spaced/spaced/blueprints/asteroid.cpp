#include <levk/scene/collision.hpp>
#include <levk/scene/scene.hpp>
#include <levk/scene/shape_renderer.hpp>
#include <levk/util/random.hpp>
#include <spaced/actors/asteroid.hpp>
#include <spaced/blueprints/asteroid.hpp>
#include <spaced/components/displacer.hpp>
#include <spaced/components/health_bar.hpp>
#include <spaced/components/hit_target.hpp>
#include <spaced/components/tumbler.hpp>
#include <spaced/game/collision_channel.hpp>

namespace spaced::blueprint {
namespace {
glm::vec3 random_position(InclusiveRange<glm::vec3> const& in) {
	return glm::vec3{
		levk::util::random_range(in.lo.x, in.hi.x),
		levk::util::random_range(in.lo.y, in.hi.y),
		levk::util::random_range(in.lo.z, in.hi.z),
	};
}
} // namespace

void Asteroid::create(levk::Entity& out) {
	Blueprint::create(out);

	auto& asteroid = out.attach(std::make_unique<spaced::Asteroid>());
	auto& collider = out.attach(std::make_unique<levk::ColliderAabb>());

	auto& mesh_entity = out.owning_scene()->spawn({.name = "mesh", .parent = out.node_id()});
	asteroid.mesh = mesh_entity.id();

	auto& renderer = mesh_entity.attach(std::make_unique<levk::ShapeRenderer>());
	auto shape = std::make_unique<levk::CubeShape>();
	auto const shape_bounds = glm::vec3{1.25f};
	shape->cube.size = shape_bounds;
	collider.aabb_size = shape->cube.size;
	collider.ignore_channels = eCC_Hostile;
	renderer.set_shape(std::move(shape));

	auto const x_dps = levk::util::random_range(dps_x.lo.to_radians().value, dps_x.hi.to_radians().value);
	auto const y_dps = levk::util::random_range(dps_y.lo.to_radians().value, dps_y.hi.to_radians().value);
	auto const z_dps = levk::util::random_range(dps_z.lo.to_radians().value, dps_z.hi.to_radians().value);
	mesh_entity.attach(std::make_unique<Tumbler>()).rot_per_s = glm::vec3{x_dps, y_dps, z_dps};

	auto& health_bar_entity = out.owning_scene()->spawn({.name = "health_bar", .parent = out.node_id()});
	auto& health_bar = health_bar_entity.attach(std::make_unique<HealthBar>());
	health_bar.y_offset = shape_bounds.y;
	asteroid.health_bar = health_bar_entity.id();

	out.attach(std::make_unique<HitTarget>()).hp = HitPoints{.total = hp, .value = hp};
	out.attach(std::make_unique<Displacer>()).velocity = velocity;

	out.transform().set_position(random_position(position));
}
} // namespace spaced::blueprint
