#include <imgui.h>
#include <levk/graphics/render_device.hpp>
#include <levk/io/serializer.hpp>
#include <levk/scene/scene.hpp>
#include <levk/scene/shape_renderer.hpp>
#include <levk/service.hpp>
#include <levk/util/fixed_string.hpp>

namespace levk {
bool ShapeRenderer::serialize(dj::Json& out) const {
	if (!shape || !material) { return false; }
	auto* serializer = Service<Serializer>::find();
	out["shape"] = serializer->serialize(*shape);
	out["material"] = serializer->serialize(*material);
	return true;
}

bool ShapeRenderer::deserialize(dj::Json const& json) {
	auto* serializer = Service<Serializer>::find();
	if (!serializer) { return false; }
	auto shape_result = serializer->deserialize_as<Shape>(json["shape"]);
	if (!shape_result) { return false; }
	auto material_result = serializer->deserialize_as<Material>(json["material"]);
	if (!material_result) { return false; }

	shape = std::move(shape_result.value);
	material = std::move(material_result.value);
	return true;
}

void ShapeRenderer::inspect(imcpp::OpenWindow w) {
	auto const label = shape ? shape->type_name() : "Shape";
	auto const* serializer = Service<Serializer>::find();
	if (auto tn = imcpp::TreeNode{FixedString{"{}###Shape", label}.c_str()}) {
		if (shape) {
			shape->inspect(w);
			if (imcpp::small_button_red("X")) { shape.reset(); }
		} else if (serializer) {
			if (ImGui::Button("Add...")) { imcpp::Popup::open("inspector.attach"); }
		}
		if (auto popup = imcpp::Popup{"inspector.attach"}) {
			assert(serializer);
			auto const type_names = serializer->type_names_by_tag(Serializer::Tag::eShape);
			for (auto const& type_name : type_names) {
				if (ImGui::Selectable(type_name.data())) { shape = serializer->try_make<Shape>(std::string{type_name}); }
			}
		}
	}
}

void ShapeRenderer::tick(Time) {
	if (!shape || !material) { return; }
	if (!primitive) {
		auto* render_device = Service<RenderDevice>::find();
		if (!render_device) { return; }
		primitive.emplace(render_device->vulkan_device());
	}
	assert(primitive.has_value());
	primitive->set_geometry(shape->make_geometry());
}

void ShapeRenderer::render(DrawList& out) const {
	auto const* entity = owning_entity();
	if (!shape || !primitive || !material || !entity) { return; }
	auto const& scene = active_scene();
	auto const drawable = Drawable{
		.primitive = primitive->vulkan_primitive(),
		.material = material.get(),
		.parent = scene.nodes().global_transform(scene.nodes().get(entity->node_id())),
	};
	out.add(drawable);
}
} // namespace levk
