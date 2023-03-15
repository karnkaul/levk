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
