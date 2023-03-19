#include <levk/asset/material_provider.hpp>
#include <levk/graphics/draw_list.hpp>
#include <levk/graphics/material.hpp>
#include <memory>

namespace levk {
void DrawList::add(Primitive::Static& primitive, Material const& material, Instanced const& instances) {
	m_drawables.push_back(Drawable::Instanced{
		.primitive = primitive,
		.material = material,
		.parent = instances.parent,
		.instances = instances.instances,
	});
}

void DrawList::add(Primitive::Dynamic& primitive, Material const& material, Instanced const& instances) {
	m_drawables.push_back(Drawable::Dynamic{
		.primitive = primitive,
		.material = material,
		.parent = instances.parent,
		.instances = instances.instances,
	});
}

void DrawList::add(Primitive::Skinned& primitive, Material const& material, Skinned const& skin) {
	m_drawables.push_back(Drawable::Skinned{
		.primitive = primitive,
		.material = material,
		.inverse_bind_matrices = skin.inverse_bind_matrices,
		.joints = skin.joints,
	});
}

void DrawList::add(StaticMesh const& mesh, Instanced const& instances, MaterialProvider& provider) {
	static auto const s_default_mat{UnlitMaterial{}};
	for (auto const& primitive : mesh.primitives) {
		if (!primitive.primitive) { continue; }
		auto const* umaterial = provider.find(primitive.material);
		auto const* material = umaterial ? umaterial->get() : &s_default_mat;
		add(*primitive.primitive, *material, instances);
	}
}

void DrawList::add(SkinnedMesh const& mesh, std::span<glm::mat4 const> joints, MaterialProvider& provider) {
	static auto const s_default_mat{LitMaterial{}};
	for (auto const& primitive : mesh.primitives) {
		if (!primitive.primitive) { continue; }
		auto const* umaterial = provider.find(primitive.material);
		auto const* material = umaterial ? umaterial->get() : &s_default_mat;
		add(*primitive.primitive, *material, {mesh.inverse_bind_matrices, joints});
	}
}
} // namespace levk
