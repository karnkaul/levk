#include <imgui.h>
#include <levk/asset/common.hpp>
#include <levk/graphics/render_device.hpp>
#include <levk/imcpp/reflector.hpp>
#include <levk/io/serializer.hpp>
#include <levk/scene/scene.hpp>
#include <levk/scene/shape_renderer.hpp>
#include <levk/service.hpp>
#include <levk/util/fixed_string.hpp>

namespace levk {
void ShapeRenderer::setup() {
	if (!m_primitive) {
		auto* render_device = Service<RenderDevice>::find();
		if (!render_device) { return; }
		m_primitive.emplace(render_device->vulkan_device());
	}
}

bool ShapeRenderer::serialize(dj::Json& out) const {
	if (!material) { return false; }
	auto* serializer = Service<Serializer>::find();
	out["material"] = serializer->serialize(*material);
	return true;
}

bool ShapeRenderer::deserialize(dj::Json const& json) {
	auto* serializer = Service<Serializer>::find();
	if (!serializer) { return false; }
	auto material_result = serializer->deserialize_as<Material>(json["material"]);
	if (!material_result) { return false; }
	material = std::move(material_result.value);
	return true;
}

void ShapeRenderer::render(DrawList& out) const {
	auto const* entity = owning_entity();
	if (!m_primitive || !material || !entity) { return; }
	auto const& scene = active_scene();
	auto const drawable = Drawable{
		.primitive = m_primitive->vulkan_primitive(),
		.material = material.get(),
		.parent = scene.nodes().global_transform(scene.nodes().get(entity->node_id())),
	};
	out.add(drawable);
}

void QuadRenderer::set_shape(Quad shape) {
	m_shape = shape;
	assert(m_primitive.has_value());
	m_primitive->set_geometry(Geometry::from(m_shape));
}

void QuadRenderer::setup() {
	ShapeRenderer::setup();
	set_shape(m_shape);
}

bool QuadRenderer::serialize(dj::Json& out) const {
	if (!ShapeRenderer::serialize(out)) { return false; }
	asset::to_json(out["shape"], m_shape);
	return true;
}

bool QuadRenderer::deserialize(dj::Json const& json) {
	if (!ShapeRenderer::deserialize(json)) { return false; }
	asset::from_json(json["shape"], m_shape);
	return true;
}

void QuadRenderer::inspect(imcpp::OpenWindow w) {
	auto shape = m_shape;
	auto const reflector = imcpp::Reflector{w};
	reflector("Size", shape.size, 0.1f, 0.001f, 1000.0f);
	reflector(shape.uv);
	reflector(shape.rgb, {false});
	reflector("Origin", shape.origin, 0.25f);
	if (shape != m_shape) { set_shape(shape); }
}

void CubeRenderer::set_shape(Cube shape) {
	m_shape = shape;
	assert(m_primitive.has_value());
	m_primitive->set_geometry(Geometry::from(m_shape));
}

void CubeRenderer::setup() {
	ShapeRenderer::setup();
	set_shape(m_shape);
}

bool CubeRenderer::serialize(dj::Json& out) const {
	if (!ShapeRenderer::serialize(out)) { return false; }
	asset::to_json(out["shape"], m_shape);
	return true;
}

bool CubeRenderer::deserialize(dj::Json const& json) {
	if (!ShapeRenderer::deserialize(json)) { return false; }
	asset::from_json(json["shape"], m_shape);
	return true;
}

void CubeRenderer::inspect(imcpp::OpenWindow w) {
	auto shape = m_shape;
	auto const reflector = imcpp::Reflector{w};
	reflector("Size", shape.size, 0.1f, 0.001f, 1000.0f);
	reflector(shape.rgb, {false});
	reflector("Origin", shape.origin, 0.25f);
	if (shape != m_shape) { set_shape(shape); }
}

void SphereRenderer::set_shape(Sphere shape) {
	m_shape = shape;
	assert(m_primitive.has_value());
	m_primitive->set_geometry(Geometry::from(m_shape));
}

void SphereRenderer::setup() {
	ShapeRenderer::setup();
	set_shape(m_shape);
}

bool SphereRenderer::serialize(dj::Json& out) const {
	if (!ShapeRenderer::serialize(out)) { return false; }
	asset::to_json(out["shape"], m_shape);
	return true;
}

bool SphereRenderer::deserialize(dj::Json const& json) {
	if (!ShapeRenderer::deserialize(json)) { return false; }
	asset::from_json(json["shape"], m_shape);
	return true;
}

void SphereRenderer::inspect(imcpp::OpenWindow w) {
	auto shape = m_shape;
	ImGui::DragFloat("Diameter", &shape.diameter, 0.25f, 0.0f, 1000.0f);
	auto ires = static_cast<int>(shape.resolution);
	if (ImGui::DragInt("Resolution", &ires, 1.0f, 1, 128)) { shape.resolution = static_cast<std::uint32_t>(ires); }
	auto const reflector = imcpp::Reflector{w};
	reflector("Origin", shape.origin, 0.25f);
	reflector(shape.rgb, {false});
	if (shape != m_shape) { set_shape(shape); }
}
} // namespace levk
