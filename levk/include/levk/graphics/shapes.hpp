#pragma once
#include <levk/graphics/shape.hpp>

namespace levk {
class QuadShape : public Shape {
  public:
	std::string_view type_name() const override { return "QuadShape"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;

	Geometry make_geometry() const override;
	void inspect(imcpp::OpenWindow w) override;

	glm::vec2 size{1.0f, 1.0f};
	UvRect uv{uv_rect_v};
};

class CubeShape : public Shape {
  public:
	std::string_view type_name() const override { return "CubeShape"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;

	Geometry make_geometry() const override;
	void inspect(imcpp::OpenWindow w) override;

	glm::vec3 size{1.0f, 1.0f, 1.0f};
};

class SphereShape : public Shape {
  public:
	std::string_view type_name() const override { return "SphereShape"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;

	Geometry make_geometry() const override;
	void inspect(imcpp::OpenWindow w) override;

	float diameter{1.0f};
	std::uint32_t resolution{16u};
};
} // namespace levk
