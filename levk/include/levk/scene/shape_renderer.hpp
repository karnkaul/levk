#pragma once
#include <levk/graphics/material.hpp>
#include <levk/graphics/shape.hpp>
#include <levk/scene/render_component.hpp>
#include <optional>

namespace levk {
class ShapeRenderer : public RenderComponent {
  public:
	std::string_view type_name() const override { return "ShapeRenderer"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;
	void inspect(imcpp::OpenWindow w) override;
	void tick(Time) override;
	void render(DrawList& out) const override;

	std::unique_ptr<Shape> shape{};
	std::unique_ptr<Material> material{};
	std::optional<DynamicPrimitive> primitive{};
};
} // namespace levk
