#include <imgui.h>
#include <levk/imcpp/asset_inspector.hpp>
#include <levk/imcpp/drag_drop.hpp>
#include <levk/util/enumerate.hpp>
#include <levk/util/fixed_string.hpp>

namespace levk::imcpp {
namespace {
template <typename Map>
PathTree build_tree(Map const& map) {
	auto builder = PathTree::Builder{};
	auto func = [&](Uri<> const& uri, auto const&) { builder.add(uri.value()); };
	map.for_each(func);
	return builder.build();
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

struct Inspector {
	void operator()(Uri<> const& uri, Texture& texture) const {
		auto const extent = texture.extent();
		TreeNode::leaf(FixedString{"{}", uri.value()}.c_str());
		if (auto drag = DragDrop::Source{}) { DragDrop::set_string("texture", uri.value()); }
		ImGui::Text("%s", FixedString{"Extent: {}x{}", extent.x, extent.y}.c_str());
		ImGui::Text("%s", FixedString{"Mip levels: {}", texture.mip_levels()}.c_str());
		ImGui::Text("%s", FixedString{"Colour Space: {}", texture.colour_space() == ColourSpace::eLinear ? "linear" : "sRGB"}.c_str());
	}

	void operator()(Uri<> const& uri, UMaterial& material) const {
		ImGui::Text("%s", FixedString{"{}", uri.value()}.c_str());
		ImGui::Text("%s", FixedString{"Vertex Shader: {}", material->vertex_shader.value()}.c_str());
		ImGui::Text("%s", FixedString{"Fragment Shader: {}", material->fragment_shader.value()}.c_str());
		if (auto* lit = dynamic_cast<LitMaterial*>(material.get())) {
			for (auto [texture, index] : enumerate(lit->textures.uris)) {
				if (auto tn = TreeNode{FixedString{"texture[{}]", index}.c_str()}) {
					FixedString<128> const label = texture ? texture.value() : "[None]";
					TreeNode::leaf(label.c_str());
					if (texture) {
						if (auto drag = DragDrop::Source{}) { DragDrop::set_string("texture", texture.value()); }
					}
					if (auto drop = DragDrop::Target{}) {
						if (auto tex = DragDrop::accept_string("texture"); !tex.empty()) { texture = tex; }
					}
				}
			}
		}
	}

	template <typename MeshT>
	void mesh_common(Uri<> const& uri, MeshT& mesh) const {
		ImGui::Text("%s", FixedString{"{}", uri.value()}.c_str());
		ImGui::Text("%s", FixedString{"Name: {}", mesh.name}.c_str());
		for (auto [primitive, index] : enumerate(mesh.primitives)) {
			if (auto tn = TreeNode{FixedString{"primitive[{}]", index}.c_str()}) {
				ImGui::Text("%s", FixedString{"Vertices: {}", primitive.primitive.vertex_count()}.c_str());
				if (primitive.material) {
					TreeNode::leaf(FixedString{"Material: {}", primitive.material.value()}.c_str());
					if (auto drag = DragDrop::Source{}) { DragDrop::set_string("material", uri.value()); }
					if (auto drop = DragDrop::Target{}) {
						if (auto const mat = DragDrop::accept_string("material"); !mat.empty()) { primitive.material = mat; }
					}
				}
			}
		}
	}

	void operator()(Uri<> const& uri, StaticMesh& mesh) const { mesh_common(uri, mesh); }

	void operator()(Uri<> const& uri, SkinnedMesh& mesh) const {
		mesh_common(uri, mesh);
		FixedString<128> const label = mesh.skeleton ? mesh.skeleton.value() : "[None]";
		TreeNode::leaf(FixedString<128>{"Skeleton: {}", label}.c_str());
		if (uri) {
			if (auto drag = DragDrop::Source{}) { DragDrop::set_string("skeleton", uri.value()); }
		}
		if (auto drop = DragDrop::Target{}) {
			// TODO: verify skeleton compatibility
		}
	}

	void operator()(Uri<> const& uri, Skeleton const& skeleton) const {
		ImGui::Text("%s", FixedString{"{}", uri.value()}.c_str());
		ImGui::Text("%s", FixedString{"Name: {}", skeleton.name}.c_str());
	}
};
} // namespace

void AssetInspector::draw_to(NotClosed<Window>, AssetProviders& out) {
	m_interact.right_clicked = m_interact.double_clicked = false;
	resource_type = list_resource_type_tabs();
	list_assets(out, resource_type);
	if (auto i = static_cast<bool>(inspecting)) {
		auto do_inspect = [&](auto& provider, char const* title) {
			ImGui::SetNextWindowSize({300.0f, 150.0f}, ImGuiCond_Once);
			if (auto right = Window{FixedString{"{}###inspect_resource", title}.c_str(), &i}) {
				if (auto* t = provider.find(inspecting.uri)) { Inspector{}(inspecting.uri, *t); }
			}
			if (!i) { inspecting = {}; }
		};
		if (inspecting.type == TypeId::make<Texture>()) { return do_inspect(out.texture(), "Texture"); }
		if (inspecting.type == TypeId::make<Material>()) { return do_inspect(out.material(), "Material"); }
		if (inspecting.type == TypeId::make<StaticMesh>()) { return do_inspect(out.static_mesh(), "StaticMesh"); }
		if (inspecting.type == TypeId::make<SkinnedMesh>()) { return do_inspect(out.skinned_mesh(), "SkinnedMesh"); }
		if (inspecting.type == TypeId::make<Skeleton>()) { return do_inspect(out.skeleton(), "Skeleton"); }
	}
}

template <typename T>
void AssetInspector::list_assets(std::string_view type_name, AssetProvider<T>& provider, PathTree& out_tree) {
	out_tree = build_tree(provider);
	for (auto const& sub_tree : out_tree.children) {
		if (walk(type_name, sub_tree)) {
			if (m_interact.right_clicked) {
				Popup::open("edit_resource");
			} else if (m_interact.double_clicked) {
				inspecting = {std::move(m_interact.interacted), TypeId::make<T>()};
				m_interact = {};
			}
		}
	}
	if (auto popup = Popup{"edit_resource"}) {
		if (ImGui::Selectable("Inspect")) {
			inspecting = {std::move(m_interact.interacted), TypeId::make<T>()};
			m_interact = {};
			Popup::close_current();
		}
		if (ImGui::Selectable("Unload")) {
			provider.remove(m_interact.interacted);
			if (inspecting.uri == m_interact.interacted) { inspecting = {}; }
			m_interact = {};
			Popup::close_current();
		}
	}
}

void AssetInspector::list_assets(AssetProviders& providers, TypeId type) {
	if (type == TypeId::make<Texture>()) { return list_assets("texture", providers.texture(), m_trees.textures); }
	if (type == TypeId::make<Material>()) { return list_assets("material", providers.material(), m_trees.materials); }
	if (type == TypeId::make<StaticMesh>()) { return list_assets("static_mesh", providers.static_mesh(), m_trees.static_meshes); }
	if (type == TypeId::make<SkinnedMesh>()) { return list_assets("skinned_mesh", providers.skinned_mesh(), m_trees.skinned_meshes); }
	if (type == TypeId::make<Skeleton>()) { return list_assets("skeleton", providers.skeleton(), m_trees.skeletons); }
}

bool AssetInspector::walk(std::string_view type_name, PathTree const& path_tree) {
	if (path_tree.name.empty()) { return false; }
	if (path_tree.is_directory()) {
		if (auto tn = TreeNode{FixedString<128>{"{}##{}", path_tree.name, type_name}.c_str()}) {
			auto ret = false;
			for (auto const& child : path_tree.children) { ret |= walk(type_name, child); }
			return ret;
		}
	} else {
		TreeNode::leaf(path_tree.name.c_str());
		bool interacted{};
		if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) { m_interact.right_clicked = interacted = true; }
		if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { m_interact.double_clicked = interacted = true; }
		if (interacted) {
			m_interact.interacted = path_tree.path;
			return true;
		}
		if (auto drag = DragDrop::Source{}) { DragDrop::set_string(type_name.data(), path_tree.path); }
	}
	return false;
}
} // namespace levk::imcpp
