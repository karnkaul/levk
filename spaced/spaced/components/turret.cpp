#include <imgui.h>
#include <levk/imcpp/common.hpp>
#include <levk/scene/scene.hpp>
#include <spaced/actors/player.hpp>
#include <spaced/actors/projectile.hpp>
#include <spaced/components/displacer.hpp>
#include <spaced/components/hit_target.hpp>
#include <spaced/components/turret.hpp>
#include <algorithm>

namespace spaced {
levk::Ptr<Projectile> Turret::fire_round() {
	if (m_cooldown > 0s) { return {}; }

	assert(round_bp);
	auto& round = owning_scene()->spawn({.parent = rounds_parent});
	round_bp->create(round);
	assert(round.find_component<Displacer>());
	round.get_component<levk::ColliderAabb>().on_collision = [this, e = round.id()](levk::ColliderAabb const& c) {
		auto& projectile = owning_scene()->get_component<Projectile>(e);
		if (auto* hit_target = c.owning_entity()->find_component<HitTarget>()) { owning_entity()->get_component<Player>().hit(*hit_target, projectile.damage); }
		projectile.owning_entity()->set_destroyed();
	};
	m_rounds.push_back(round.id());
	round.transform().set_position(owning_entity()->transform().position() + glm::vec3{round_bp->size, 0.0f});

	m_cooldown = cooldown;

	return &round.get_component<Projectile>();
}

void Turret::inspect(levk::imcpp::OpenWindow w) {
	if (auto tn = levk::imcpp::TreeNode{"Round"}) { round_bp->inspect(w); }
	auto value = cooldown.count();
	if (ImGui::DragFloat("Cooldown", &value, 0.025f, 0.1f, 10.0f)) { cooldown = levk::Duration{value}; }
}

void Turret::tick(levk::Duration dt) {
	if (m_cooldown > 0s) { m_cooldown -= dt; }

	std::erase_if(m_rounds, [this](auto const id) { return !owning_scene()->find_entity(id); });
}
} // namespace spaced
