#include <imgui.h>
#include <levk/imcpp/reflector.hpp>
#include <levk/scene/collision.hpp>
#include <levk/scene/shape_renderer.hpp>
#include <spaced/actors/projectile.hpp>
#include <spaced/blueprints/projectile.hpp>
#include <spaced/components/displacer.hpp>
#include <spaced/game/collision_channel.hpp>

namespace spaced::blueprint {
void Projectile::create(levk::Entity& out) {
	Blueprint::create(out);

	auto& projectile = out.attach(std::make_unique<spaced::Projectile>());

	auto& sr = out.attach(std::make_unique<levk::ShapeRenderer>());
	auto shape = std::make_unique<levk::CubeShape>();
	shape->cube.size = {size, 1.0f};
	projectile.mesh = out.id();

	auto& collider = out.attach(std::make_unique<levk::ColliderAabb>());
	collider.aabb_size = shape->cube.size;
	collider.ignore_channels = eCC_Player;
	sr.set_shape(std::move(shape));

	auto& displacer = out.attach(std::make_unique<Displacer>());
	displacer.velocity.x = x_speed;
}

void Projectile::inspect(levk::imcpp::OpenWindow w) {
	auto const reflector = levk::imcpp::Reflector{w};
	reflector("Size", size);
	ImGui::DragFloat("X Speed", &x_speed, 0.25f, 2.0f, 1000.0f);
}
} // namespace spaced::blueprint
