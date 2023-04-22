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
	Shape const& shape() const;

	void refresh_geometry();

	void setup() override;
	void render(DrawList& out) const override;
	std::unique_ptr<Attachment> to_attachment() const override;

	std::vector<Transform> instances{};

  protected:
	std::optional<DynamicPrimitive> m_primitive{};
	std::unique_ptr<Shape> m_shape{std::make_unique<CubeShape>()};
};
} // namespace levk
