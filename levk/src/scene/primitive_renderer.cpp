#include <levk/scene/primitive_renderer.hpp>
#include <levk/scene/scene.hpp>
#include <levk/scene/scene_manager.hpp>
#include <levk/service.hpp>
#include <levk/util/fixed_string.hpp>
#include <levk/util/visitor.hpp>

namespace levk {
void PrimitiveRenderer::render(RenderList& out) const {
	if (!primitive) { return; }
	auto* entity = owning_entity();
	if (!entity) { return; }
	auto const locator = active_scene().node_locator();
	auto const visitor = [&](auto const& primitive) {
		auto const drawable = Drawable{
			.primitive = primitive.vulkan_primitive(),
			.material = &material,
			.parent = locator.global_transform(locator.get(entity->node_id())),
			.instances = instances,
		};
		if (material.is_opaque()) {
			out.opaque.add(drawable);
		} else {
			out.transparent.add(drawable);
		}
	};
	std::visit(visitor, *primitive);
}

void PrimitiveRenderer::inspect(imcpp::OpenWindow) {
	if (!primitive) { return; }
	auto* scene_manager = Service<SceneManager>::find();
	if (!scene_manager) { return; }
	auto const func = [](std::string_view type, levk::Primitive const& prim) {
		imcpp::TreeNode::leaf(FixedString{"Primitive::{}", type}.c_str());
		imcpp::TreeNode::leaf(FixedString{"Vertices: {}", prim.vertex_count()}.c_str());
		imcpp::TreeNode::leaf(FixedString{"Indices: {}", prim.index_count()}.c_str());
	};
	auto const visitor = Visitor{
		[func](StaticPrimitive const& prim) { func("Static", prim); },
		[func](DynamicPrimitive const& prim) { func("Dynamic", prim); },
	};
	std::visit(visitor, *primitive);
}
} // namespace levk
