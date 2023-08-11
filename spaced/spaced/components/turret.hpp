#pragma once
#include <spaced/actors/projectile.hpp>
#include <spaced/blueprints/projectile.hpp>
#include <span>

namespace spaced {
class Turret : public levk::Component {
  public:
	levk::Id<levk::Node> rounds_parent{};
	levk::Ptr<blueprint::Projectile> round_bp{};
	levk::Duration cooldown{0.5s};

	levk::Ptr<Projectile> fire_round();
	void inspect(levk::imcpp::OpenWindow w);

	std::span<levk::EntityId const> spawned_rounds() const { return m_rounds; }

  protected:
	void tick(levk::Duration dt) override;

	std::vector<levk::EntityId> m_rounds{};
	levk::Duration m_cooldown{};
};
} // namespace spaced
