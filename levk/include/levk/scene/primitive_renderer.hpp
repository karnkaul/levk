#pragma once
#include <levk/graphics/material.hpp>
#include <levk/scene/render_component.hpp>
#include <optional>

namespace levk {
struct PrimitiveRenderer : RenderComponent {
	using Primitive = std::variant<StaticPrimitive, DynamicPrimitive>;

	std::vector<Transform> instances{};
	UnlitMaterial material{};
	std::optional<Primitive> primitive{};

	std::string_view type_name() const override { return "PrimitiveRenderer"; }
	bool serialize(dj::Json&) const override { return false; }
	bool deserialize(dj::Json const&) override { return false; }
	void inspect(imcpp::OpenWindow) override;
	void render(DrawList& out) const final;
};
} // namespace levk
