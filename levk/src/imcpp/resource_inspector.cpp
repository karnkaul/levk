#include <imgui.h>
#include <levk/imcpp/resource_inspector.hpp>
#include <levk/util/fixed_string.hpp>

namespace levk::imcpp {
namespace {
TypeId list_resource_type_tabs() {
	auto ret = TypeId::make<Texture>();
	if (auto bar = TabBar{"Resource Type"}) {
		if (auto item = TabBar::Item{bar, "Textures"}) { ret = TypeId::make<Texture>(); }
		if (auto item = TabBar::Item{bar, "Materials"}) { ret = TypeId::make<Material>(); }
	}
	return ret;
}

template <typename T>
void list_resources(ResourceMap<T> const& map, ResourceInspector::Inspect& out_inspect) {
	auto func = [&](Uri const& uri, auto const&) {
		bool const selected = out_inspect.uri == uri;
		if (ImGui::Selectable(uri.value().data(), selected)) { out_inspect = {TypeId::make<T>(), uri}; }
	};
	map.for_each(func);
}

void list_resources(Resources const& resources, TypeId type, ResourceInspector::Inspect& out_inspect) {
	if (auto lb = ListBox{"##List", {-FLT_MIN, -FLT_MIN}}) {
		if (type == TypeId::make<Texture>()) { return list_resources(resources.render.textures, out_inspect); }
		if (type == TypeId::make<Material>()) { return list_resources(resources.render.materials, out_inspect); }
	}
}

void inspect_texture(Uri const& uri, Texture& texture) {
	auto const extent = texture.extent();
	TreeNode::leaf("Texture");
	ImGui::Text("%s", FixedString{"{}", uri.value()}.c_str());
	ImGui::Text("%s", FixedString{"Extent: {}x{}", extent.x, extent.y}.c_str());
	ImGui::Text("%s", FixedString{"Mip levels: {}", texture.mip_levels()}.c_str());
	ImGui::Text("%s", FixedString{"Colour Space: {}", texture.colour_space() == ColourSpace::eLinear ? "linear" : "sRGB"}.c_str());
}

void inspect_material(Uri const& uri, Material const& material) {
	TreeNode::leaf("Material");
	ImGui::Text("%s", FixedString{"{}", uri.value()}.c_str());
	ImGui::Text("%s", FixedString{"Shader: {}", material.shader_id()}.c_str());
}
} // namespace

void ResourceInspector::inspect(NotClosed<Window> window, Resources& out) {
	resource_type = list_resource_type_tabs();
	auto const size = glm::vec2{ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y * 0.5f};
	ImGuiWindowFlags flags = ImGuiWindowFlags_HorizontalScrollbar;
	if (auto top = Window{window, "List", size, {true}, flags}) { list_resources(out, resource_type, inspecting); }
	if (inspecting) {
		if (auto right = Window{window, "Inspect", size, {true}, flags}) {
			if (inspecting.type == TypeId::make<Texture>()) {
				if (auto* tex = out.render.textures.find(inspecting.uri)) { return inspect_texture(inspecting.uri, *tex); }
			}
			if (inspecting.type == TypeId::make<Material>()) {
				if (auto* mat = out.render.materials.find(inspecting.uri)) { return inspect_material(inspecting.uri, *mat); }
			}
		}
	}
}
} // namespace levk::imcpp
