#include <imgui.h>
#include <levk/imcpp/resource_inspector.hpp>
#include <levk/util/enumerate.hpp>
#include <levk/util/fixed_string.hpp>

namespace levk::imcpp {
namespace {
template <typename Map>
PathTree build_tree(Map const& map) {
	auto builder = PathTree::Builder{};
	auto func = [&](Uri const& uri, auto const&) { builder.add(uri.value()); };
	map.for_each(func);
	return builder.build();
}

bool walk(std::string_view type_name, PathTree const& path_tree, std::string& out_right_clicked) {
	if (path_tree.name.empty()) { return false; }
	if (path_tree.is_directory()) {
		if (auto tn = TreeNode{FixedString<128>{"{}##{}", path_tree.name, type_name}.c_str()}) {
			auto ret = false;
			for (auto const& child : path_tree.children) { ret |= walk(type_name, child, out_right_clicked); }
			return ret;
		}
	} else {
		TreeNode::leaf(path_tree.name.c_str());
		if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
			out_right_clicked = path_tree.path;
			return true;
		}
		if (auto drag = DragDropSource{}) {
			ImGui::SetDragDropPayload(type_name.data(), path_tree.path.c_str(), path_tree.path.size());
			ImGui::Text("%s", path_tree.path.c_str());
		}
	}
	return false;
}
} // namespace

namespace {
TypeId list_resource_type_tabs() {
	auto ret = TypeId::make<Texture>();
	if (auto bar = TabBar{"Resource Type"}) {
		if (auto item = TabBar::Item{bar, "Textures"}) { ret = TypeId::make<Texture>(); }
		if (auto item = TabBar::Item{bar, "Materials"}) { ret = TypeId::make<Material>(); }
		if (auto item = TabBar::Item{bar, "Static Meshes"}) { ret = TypeId::make<StaticMesh>(); }
		if (auto item = TabBar::Item{bar, "Skinned Meshes"}) { ret = TypeId::make<SkinnedMesh>(); }
		if (auto item = TabBar::Item{bar, "Skeletons"}) { ret = TypeId::make<Skeleton>(); }
	}
	return ret;
}

void inspect_texture(Uri const& uri, Texture& texture) {
	auto const extent = texture.extent();
	TreeNode::leaf(FixedString{"{}", uri.value()}.c_str());
	if (auto drag = DragDropSource{}) {
		auto str = uri.value();
		ImGui::SetDragDropPayload("texture", str.data(), str.size());
		ImGui::Text("%s", str.data());
	}
	ImGui::Text("%s", FixedString{"Extent: {}x{}", extent.x, extent.y}.c_str());
	ImGui::Text("%s", FixedString{"Mip levels: {}", texture.mip_levels()}.c_str());
	ImGui::Text("%s", FixedString{"Colour Space: {}", texture.colour_space() == ColourSpace::eLinear ? "linear" : "sRGB"}.c_str());
}

void inspect_material(Uri const& uri, Material const& material) {
	ImGui::Text("%s", FixedString{"{}", uri.value()}.c_str());
	ImGui::Text("%s", FixedString{"Shader: {}", material.shader_id()}.c_str());
	if (auto* lit = material.as<LitMaterial>()) {
		for (auto [texture, index] : enumerate(lit->textures.textures)) {
			if (auto tn = TreeNode{FixedString{"texture[{}]", index}.c_str()}) {
				auto& uri = lit->textures.textures[index].uri;
				std::string_view const label = uri ? uri.value() : "[None]";
				TreeNode::leaf(label.data());
				if (uri) {
					if (auto drag = DragDropSource{}) {
						auto str = uri.value();
						ImGui::SetDragDropPayload("texture", str.data(), str.size());
						ImGui::Text("%s", str.data());
					}
				}
				if (auto drop = DragDropTarget{}) {
					if (auto const* payload = ImGui::AcceptDragDropPayload("texture")) {
						uri = std::string{static_cast<char const*>(payload->Data), static_cast<std::size_t>(payload->DataSize)};
					}
				}
			}
		}
	}
}

void inspect_static_mesh(Uri const& uri, StaticMesh& static_mesh) {
	ImGui::Text("%s", FixedString{"{}", uri.value()}.c_str());
	ImGui::Text("%s", FixedString{"{}", static_mesh.name}.c_str());
	for (auto [primitive, index] : enumerate(static_mesh.primitives)) {
		if (auto tn = TreeNode{FixedString{"primitive[{}]", index}.c_str()}) {
			ImGui::Text("%s", FixedString{"Vertices: {}", primitive.geometry.vertex_count()}.c_str());
			if (primitive.material) {
				TreeNode::leaf(FixedString{"Material: {}", primitive.material.value()}.c_str());
				if (auto drag = DragDropSource{}) {
					auto str = uri.value();
					ImGui::SetDragDropPayload("material", str.data(), str.size());
					ImGui::Text("%s", str.data());
				}
				if (auto drop = DragDropTarget{}) {
					if (auto const* payload = ImGui::AcceptDragDropPayload("material")) {
						primitive.material = std::string{static_cast<char const*>(payload->Data), static_cast<std::size_t>(payload->DataSize)};
					}
				}
			}
		}
	}
}
} // namespace

void ResourceInspector::inspect(NotClosed<Window>, Resources& out) {
	resource_type = list_resource_type_tabs();
	list_resources(out, resource_type);
	if (auto i = static_cast<bool>(inspecting)) {
		auto do_inspect = [&](auto func, auto& map, char const* title) {
			ImGui::SetNextWindowSize({300.0f, 150.0f}, ImGuiCond_Once);
			if (auto right = Window{FixedString{"{}###inspect_resource", title}.c_str(), &i}) {
				if (auto* t = map.find(inspecting.uri)) { func(inspecting.uri, *t); }
			}
			if (!i) {
				//
				inspecting = {};
			}
		};
		if (inspecting.type == TypeId::make<Texture>()) { return do_inspect(&inspect_texture, out.render.textures, "Texture"); }
		if (inspecting.type == TypeId::make<Material>()) { return do_inspect(&inspect_material, out.render.materials, "Material"); }
		if (inspecting.type == TypeId::make<StaticMesh>()) { return do_inspect(&inspect_static_mesh, out.render.static_meshes, "Material"); }
	}
}

template <typename T>
void ResourceInspector::list_resources(std::string_view type_name, ResourceMap<T>& map, PathTree& out_tree, std::uint64_t signature) {
	if (m_signature != signature) { out_tree = build_tree(map); }
	for (auto const& sub_tree : out_tree.children) {
		if (walk(type_name, sub_tree, m_right_clicked)) { Popup::open("edit_resource"); }
	}
	if (auto popup = Popup{"edit_resource"}) {
		if (ImGui::Selectable("Inspect")) { inspecting = {std::exchange(m_right_clicked, {}), TypeId::make<T>()}; }
		if (ImGui::Selectable("Unload")) {
			map.remove(m_right_clicked);
			m_right_clicked.clear();
		}
	}
}

void ResourceInspector::list_resources(Resources& resources, TypeId type) {
	auto const signature = resources.signature();
	if (type == TypeId::make<Texture>()) { return list_resources("texture", resources.render.textures, m_trees.textures, signature); }
	if (type == TypeId::make<Material>()) { return list_resources("material", resources.render.materials, m_trees.materials, signature); }
	if (type == TypeId::make<StaticMesh>()) { return list_resources("static_mesh", resources.render.static_meshes, m_trees.static_meshes, signature); }
	if (type == TypeId::make<SkinnedMesh>()) { return list_resources("skinned_mesh", resources.render.skinned_meshes, m_trees.skinned_meshes, signature); }
	if (type == TypeId::make<Skeleton>()) { return list_resources("skeletons", resources.render.skeletons, m_trees.skeletons, signature); }
	m_signature = signature;
}
} // namespace levk::imcpp
