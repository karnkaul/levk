#pragma once
#include <levk/graphics/geometry.hpp>
#include <levk/io/serializable.hpp>
#include <levk/level/inspectable.hpp>
#include <levk/uri.hpp>
#include <memory>

namespace levk {
class Material;

struct Shape : Serializable, Inspectable {
	virtual Geometry make_geometry() const = 0;

	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& out) override;
	void inspect(imcpp::OpenWindow w) override;

	Uri<Material> material_uri{};
};

using UShape = std::unique_ptr<Shape>;

struct QuadShape : Shape {
	Geometry make_geometry() const override { return Geometry::from(quad); }

	std::string_view type_name() const override { return "QuadShape"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& out) override;
	void inspect(imcpp::OpenWindow w) override;

	Quad quad{};
};

struct CubeShape : Shape {
	using Shape::Shape;

	Geometry make_geometry() const override { return Geometry::from(cube); }

	std::string_view type_name() const override { return "CubeShape"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& out) override;
	void inspect(imcpp::OpenWindow w) override;

	Cube cube{};
};

struct SphereShape : Shape {
	using Shape::Shape;

	Geometry make_geometry() const override { return Geometry::from(sphere); }

	std::string_view type_name() const override { return "SphereShape"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& out) override;
	void inspect(imcpp::OpenWindow w) override;

	Sphere sphere{};
};
} // namespace levk
