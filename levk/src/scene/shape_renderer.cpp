#include <levk/asset/asset_providers.hpp>
#include <levk/asset/material_provider.hpp>
#include <levk/graphics/render_device.hpp>
#include <levk/level/attachments.hpp>
#include <levk/scene/entity.hpp>
#include <levk/scene/shape_renderer.hpp>
#include <levk/service.hpp>

namespace levk {
void ShapeRenderer::set_shape(std::unique_ptr<Shape> shape) {
	if (!shape) { return; }
	m_shape = std::move(shape);
	if (!m_primitive) { return; }
	m_primitive->set_geometry(m_shape->make_geometry());
}

Shape const& ShapeRenderer::shape() const {
	assert(m_shape);
	return *m_shape;
}

void ShapeRenderer::refresh_geometry() {
	if (!m_primitive) { return; }
	m_primitive->set_geometry(m_shape->make_geometry());
}

void ShapeRenderer::setup() {
	m_primitive = DynamicPrimitive{render_device().vulkan_device()};
	if (!m_shape) { m_shape = std::make_unique<CubeShape>(); }
	refresh_geometry();
}

void ShapeRenderer::render(DrawList& out) const {
	if (!m_primitive || !m_shape) { return; }
	auto* asset_providers = Service<AssetProviders>::find();
	if (!asset_providers) { return; }
	static std::unique_ptr<Material> const s_default_mat{std::make_unique<LitMaterial>()};
	auto* entity = owning_entity();
	auto const matrix = entity ? entity->transform().matrix() : glm::mat4{1.0f};
	out.add(Drawable{
		.primitive = m_primitive->vulkan_primitive(),
		.material = asset_providers->material().get(m_shape->material_uri, s_default_mat).get(),
		.parent = matrix,
		.instances = instances,
	});
}

std::unique_ptr<Attachment> ShapeRenderer::to_attachment() const {
	auto* serializer = Service<Serializer>::find();
	if (!m_shape || !serializer) { return {}; }
	auto ret = std::make_unique<ShapeAttachment>();
	ret->shape = serializer->serialize(*m_shape);
	ret->instances = instances;
	ret->material_uri = m_shape->material_uri;
	return ret;
}
} // namespace levk
