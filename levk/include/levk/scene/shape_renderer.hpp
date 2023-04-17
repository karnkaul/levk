#pragma once
#include <levk/graphics/primitive.hpp>
#include <levk/level/shape.hpp>
#include <levk/scene/component.hpp>
#include <levk/transform.hpp>
#include <optional>

namespace levk {
class ShapeRenderer : public RenderComponent {
  public:
	void set_shape(std::unique_ptr<Shape> shape);
	Ptr<Shape const> shape() const { return m_shape.get(); }

	void refresh_geometry();

	void setup() override;
	void render(DrawList& out) const override;

	std::vector<Transform> instances{};

  protected:
	std::optional<DynamicPrimitive> m_primitive{};
	std::unique_ptr<Shape> m_shape{};
};
} // namespace levk
