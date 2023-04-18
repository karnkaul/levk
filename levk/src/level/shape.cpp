#include <imgui.h>
#include <levk/asset/asset_io.hpp>
#include <levk/imcpp/drag_drop.hpp>
#include <levk/imcpp/reflector.hpp>
#include <levk/io/serializer.hpp>
#include <levk/level/shape.hpp>
#include <levk/service.hpp>
#include <levk/util/enumerate.hpp>
#include <levk/util/fixed_string.hpp>

namespace levk {
bool Shape::serialize(dj::Json& out) const {
	if (!material_uri) { return false; }
	out["material_uri"] = material_uri.value();
	return true;
}

bool Shape::deserialize(dj::Json const& json) {
	material_uri = json["material_uri"].as<std::string>();
	return true;
}

void Shape::inspect(imcpp::OpenWindow) {
	if (auto tn = imcpp::TreeNode{"Material"}) {
		auto const label = material_uri ? material_uri.value().c_str() : "[None]";
		imcpp::TreeNode::leaf(label);
		if (material_uri) {
			if (auto drag = imcpp::DragDrop::Source{}) { imcpp::DragDrop::set_string("material", material_uri.value()); }
			if (ImGui::SmallButton("Unset")) { material_uri = {}; }
		}
		if (auto drop = imcpp::DragDrop::Target{}) {
			if (auto mat = imcpp::DragDrop::accept_string("material"); !mat.empty()) { material_uri = mat; }
		}
	}
}

bool QuadShape::serialize(dj::Json& out) const {
	if (!Shape::serialize(out)) { return false; }
	asset::to_json(out["quad"], quad);
	return true;
}

bool QuadShape::deserialize(dj::Json const& json) {
	if (!Shape::deserialize(json)) { return false; }
	asset::from_json(json["quad"], quad);
	return true;
}

void QuadShape::inspect(imcpp::OpenWindow w) {
	Shape::inspect(w);
	auto const reflector = imcpp::Reflector{w};
	reflector("Size", quad.size, 0.1f, 0.001f, 1000.0f);
	reflector(quad.uv);
	reflector(quad.rgb, {false});
	reflector("Origin", quad.origin, 0.25f);
}

bool CubeShape::serialize(dj::Json& out) const {
	if (!Shape::serialize(out)) { return false; }
	asset::to_json(out["cube"], cube);
	return true;
}

bool CubeShape::deserialize(dj::Json const& json) {
	if (!Shape::deserialize(json)) { return false; }
	asset::from_json(json["cube"], cube);
	return true;
}

void CubeShape::inspect(imcpp::OpenWindow w) {
	Shape::inspect(w);
	auto const reflector = imcpp::Reflector{w};
	reflector("Size", cube.size, 0.1f, 0.001f, 1000.0f);
	reflector(cube.rgb, {false});
	reflector("Origin", cube.origin, 0.25f);
}

bool SphereShape::serialize(dj::Json& out) const {
	if (!Shape::serialize(out)) { return false; }
	asset::to_json(out["sphere"], sphere);
	return true;
}

bool SphereShape::deserialize(dj::Json const& json) {
	if (!Shape::deserialize(json)) { return false; }
	asset::from_json(json["sphere"], sphere);
	return true;
}

void SphereShape::inspect(imcpp::OpenWindow w) {
	Shape::inspect(w);
	ImGui::DragFloat("Diameter", &sphere.diameter, 0.25f, 0.0f, 1000.0f);
	auto ires = static_cast<int>(sphere.resolution);
	if (ImGui::DragInt("Resolution", &ires, 1.0f, 1, 128)) { sphere.resolution = static_cast<std::uint32_t>(ires); }
	auto const reflector = imcpp::Reflector{w};
	reflector("Origin", sphere.origin, 0.25f);
	reflector(sphere.rgb, {false});
}
} // namespace levk
