#include <imgui.h>
#include <levk/asset/common.hpp>
#include <levk/graphics/shapes.hpp>
#include <levk/imcpp/reflector.hpp>

namespace levk {
bool QuadShape::serialize(dj::Json& out) const {
	if (!Shape::serialize(out)) { return false; }
	asset::to_json(out["size"], size);
	asset::to_json(out["uv_rect"], uv);
	return true;
}

bool QuadShape::deserialize(dj::Json const& json) {
	if (!Shape::deserialize(json)) { return false; }
	asset::from_json(json["size"], size);
	asset::from_json(json["uv_rect"], uv);
	return true;
}

Geometry QuadShape::make_geometry() const {
	auto const qci = QuadCreateInfo{
		.size = size,
		.rgb = rgb.to_vec4(),
		.origin = origin,
		.uv = uv,
	};
	return make_quad(qci);
}

void QuadShape::inspect(imcpp::OpenWindow w) {
	Shape::inspect(w);
	auto const reflector = imcpp::Reflector{w};
	reflector("Size", size, 0.1f, 0.001f, 1000.0f);
	reflector(uv);
}

bool CubeShape::serialize(dj::Json& out) const {
	if (!Shape::serialize(out)) { return false; }
	asset::to_json(out["size"], size);
	return true;
}

bool CubeShape::deserialize(dj::Json const& json) {
	if (!Shape::deserialize(json)) { return false; }
	asset::from_json(json["size"], size);
	return true;
}

Geometry CubeShape::make_geometry() const { return make_cube(size, rgb.to_vec4(), origin); }

void CubeShape::inspect(imcpp::OpenWindow w) {
	Shape::inspect(w);
	auto const reflector = imcpp::Reflector{w};
	reflector("Size", size, 0.1f, 0.001f, 1000.0f);
}

bool SphereShape::serialize(dj::Json& out) const {
	if (!Shape::serialize(out)) { return false; }
	out["diameter"] = diameter;
	out["resolution"] = resolution;
	return true;
}

bool SphereShape::deserialize(dj::Json const& json) {
	if (!Shape::deserialize(json)) { return false; }
	diameter = json["diameter"].as<float>(diameter);
	resolution = json["resolution"].as<std::uint32_t>(resolution);
	return true;
}

Geometry SphereShape::make_geometry() const { return make_cubed_sphere(diameter, resolution, rgb.to_vec4(), origin); }

void SphereShape::inspect(imcpp::OpenWindow w) {
	Shape::inspect(w);
	ImGui::DragFloat("Diameter", &diameter, 0.25f, 0.0f, 1000.0f);
	auto ires = static_cast<int>(resolution);
	if (ImGui::DragInt("Resolution", &ires, 1.0f, 1, 128)) { resolution = static_cast<std::uint32_t>(ires); }
}
} // namespace levk
