#pragma once
#include <levk/scene/shape_renderer.hpp>

namespace spaced {
class HealthBar : public levk::ShapeRenderer {
  public:
	float n_value{1.0f};
	float y_offset{2.0f};
	float z_index{1.0f};
	glm::vec2 size{2.0f, 0.2f};
	levk::Rgba tint{levk::red_v};

  private:
	void tick(levk::Duration) override;
};
} // namespace spaced
