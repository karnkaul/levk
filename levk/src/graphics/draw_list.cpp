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

void DrawList::add(NotNull<DynamicPrimitive const*> primitive, NotNull<Material const*> material, Instances const& instances) {
	m_drawables.push_back(Drawable{
		.primitive = primitive->vulkan_primitive(),
		.material = material,
		.parent = instances.parent,
		.instances = instances.instances,
	});
}

void DrawList::add(NotNull<SkinnedPrimitive const*> primitive, NotNull<Material const*> material, Skin const& skin) {
	m_drawables.push_back(Drawable{
		.primitive = primitive->vulkan_primitive(),
		.material = material,
		.inverse_bind_matrices = skin.inverse_bind_matrices,
		.joints = skin.joints,
	});
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

void DrawList::add(NotNull<SkinnedMesh const*> mesh, std::span<glm::mat4 const> joints, MaterialProvider& provider) {
	static auto const s_default_mat{LitMaterial{}};
	for (auto const& primitive : mesh->primitives) {
		auto const* umaterial = provider.find(primitive.material);
		auto const* material = umaterial ? umaterial->get() : &s_default_mat;
		add(&primitive.primitive, material, {mesh->inverse_bind_matrices, joints});
	}
}

void DrawList::add(NotNull<StaticPrimitive const*> primitive, NotNull<Material const*> material, Transform const& transform) {
	add(primitive, material, {transform.matrix()});
}

void DrawList::add(NotNull<DynamicPrimitive const*> primitive, NotNull<Material const*> material, Transform const& transform) {
	add(primitive, material, {transform.matrix()});
}
} // namespace levk
