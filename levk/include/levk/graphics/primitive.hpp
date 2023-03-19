#pragma once
#include <levk/geometry.hpp>

namespace levk {
enum class Topology : std::uint8_t { ePointList, eLineList, eLineStrip, eTriangleList, eTriangleStrip, eTriangleFan };

struct Primitive {
	struct Static;
	struct Dynamic;
	struct Skinned;

	virtual ~Primitive() = default;

	Topology topology{Topology::eTriangleList};

	virtual std::uint32_t vertex_count() const = 0;
	virtual std::uint32_t index_count() const = 0;
};

struct Primitive::Static : Primitive {};

struct Primitive::Dynamic : Primitive {
	virtual void set_geometry(Geometry::Packed geometry) = 0;
	virtual Geometry::Packed const& get_geometry() const = 0;
};

struct Primitive::Skinned : Primitive {
	virtual std::uint32_t joint_count() const = 0;
};
} // namespace levk
