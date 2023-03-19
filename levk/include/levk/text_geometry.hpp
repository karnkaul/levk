#pragma once
#include <levk/geometry.hpp>
#include <levk/uri.hpp>

namespace levk {
class Texture;

struct TextGeometry {
	Geometry geometry{};
	Uri<Texture> atlas{};
};
} // namespace levk

#include <levk/graphics/material.hpp>
#include <levk/graphics/primitive.hpp>

namespace levk {
struct TextPrimitive {
	std::unique_ptr<Primitive::Dynamic> primitive{};
	UnlitMaterial material{};
};
} // namespace levk
