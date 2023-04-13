#pragma once
#include <levk/graphics/geometry.hpp>
#include <levk/graphics/material.hpp>
#include <levk/scene/render_component.hpp>
#include <optional>

namespace levk {
class ShapeRenderer : public RenderComponent {
  public:
	ShapeRenderer(std::unique_ptr<Material> material = std::make_unique<LitMaterial>()) : material(std::move(material)) {}

	void setup() override;
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& out) override;
	void render(DrawList& out) const override;

	std::unique_ptr<Material> material{};

  protected:
	std::optional<DynamicPrimitive> m_primitive{};
};

class QuadRenderer : public ShapeRenderer {
  public:
	QuadRenderer() : ShapeRenderer(std::make_unique<UnlitMaterial>()) {}

	void set_shape(Quad shape);
	Quad const& get_shape() const { return m_shape; }

	void setup() override;
	std::string_view type_name() const override { return "QuadRenderer"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& out) override;
	void inspect(imcpp::OpenWindow w) override;

  protected:
	Quad m_shape{};
};

class CubeRenderer : public ShapeRenderer {
  public:
	void set_shape(Cube shape);
	Cube const& get_shape() const { return m_shape; }

	void setup() override;
	std::string_view type_name() const override { return "CubeRenderer"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& out) override;
	void inspect(imcpp::OpenWindow w) override;

  protected:
	Cube m_shape{};
};

class SphereRenderer : public ShapeRenderer {
  public:
	void set_shape(Sphere shape);
	Sphere const& get_shape() const { return m_shape; }

	void setup() override;
	std::string_view type_name() const override { return "SphereRenderer"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& out) override;
	void inspect(imcpp::OpenWindow w) override;

  protected:
	Sphere m_shape{};
};
} // namespace levk
