#include <levk/asset/material_provider.hpp>
#include <levk/graphics/draw_list.hpp>
#include <levk/graphics/material.hpp>
#include <memory>

namespace levk {
void DrawList::add(NotNull<StaticPrimitive const*> primitive, NotNull<Material const*> material, Instances const& instances) {
	m_drawables.push_back(Drawable{
		.primitive = primitive->vulkan_primitive(),
		.material = material,
		.parent = instances.parent,
		.instances = instances.instances,
	});
}

void DrawList::add(NotNull<StaticPrimitive const*> primitive, NotNull<Material const*> material, Transform const& transform) {
	add(primitive, material, {transform.matrix()});
}

void DrawList::add(NotNull<DynamicPrimitive const*> primitive, NotNull<Material const*> material, Instances const& instances) {
	m_drawables.push_back(Drawable{
		.primitive = primitive->vulkan_primitive(),
		.material = material,
		.parent = instances.parent,
		.instances = instances.instances,
	});
}

void DrawList::add(NotNull<DynamicPrimitive const*> primitive, NotNull<Material const*> material, Transform const& transform) {
	add(primitive, material, {transform.matrix()});
}

void DrawList::add(NotNull<StaticMesh const*> mesh, Instances const& instances, MaterialProvider& provider) {
	static auto const s_default_mat{UnlitMaterial{}};
	for (auto const& primitive : mesh->primitives) {
		auto const* umaterial = provider.find(primitive.material);
		auto const* material = umaterial ? umaterial->get() : &s_default_mat;
		add(&primitive.primitive, material, instances);
	}
}

void DrawList::add(NotNull<StaticMesh const*> mesh, glm::mat4 const& transform, MaterialProvider& provider) { add(mesh, Instances{transform}, provider); }

void DrawList::add(NotNull<SkinnedMesh const*> mesh, std::span<glm::mat4 const> joints, Instances const& instances, MaterialProvider& provider) {
	static auto const s_default_mat{LitMaterial{}};
	auto skin_index = m_skins.size();
	m_skins.push_back(Skin{mesh->inverse_bind_matrices, joints});
	for (auto const& primitive : mesh->primitives) {
		auto const* umaterial = provider.find(primitive.material);
		auto const* material = umaterial ? umaterial->get() : &s_default_mat;
		m_drawables.push_back(Drawable{
			.primitive = primitive.primitive.vulkan_primitive(),
			.material = material,
			.skin_index = skin_index,
			.parent = instances.parent,
			.instances = instances.instances,
		});
	}
}

void DrawList::add(NotNull<SkinnedMesh const*> mesh, std::span<glm::mat4 const> joints, glm::mat4 const& transform, MaterialProvider& provider) {
	add(mesh, joints, Instances{transform}, provider);
}

void DrawList::add(Drawable drawable) {
	assert(!drawable.skin_index || *drawable.skin_index < m_skins.size());
	m_drawables.push_back(drawable);
}

void DrawList::merge(DrawList const& rhs) { std::copy(rhs.m_drawables.begin(), rhs.m_drawables.end(), std::back_inserter(m_drawables)); }
} // namespace levk
