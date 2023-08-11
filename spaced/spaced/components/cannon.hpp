#pragma once
#include <glm/vec2.hpp>
#include <levk/graphics/rgba.hpp>
#include <levk/imcpp/common.hpp>
#include <spaced/game/actor.hpp>

namespace spaced {
class Cannon : public Actor {
  public:
	glm::vec2 target{};

	levk::Duration duration{2s};
	levk::Rgba tint{levk::cyan_v};
	float dps{100.0f};
	float height{0.05f};

	bool fire_beam();
	bool is_firing() const { return m_remain > 0s; }

	void inspect(levk::imcpp::OpenWindow w);

  private:
	void setup() final;
	void tick(levk::Duration dt) final;

	levk::Duration m_remain{};
};
} // namespace spaced
