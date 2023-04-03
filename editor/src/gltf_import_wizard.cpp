#include <imgui.h>
#include <gltf_import_wizard.hpp>
#include <levk/scene/scene.hpp>
#include <levk/util/enumerate.hpp>
#include <levk/util/fixed_string.hpp>
#include <filesystem>
#include <ranges>

namespace levk::imcpp {
namespace fs = std::filesystem;

struct GltfImportWizard::Walk {
	legsmi::MeshImporter mesh_importer;
	legsmi::SceneImporter scene_importer;
	InputText<256>& export_uri;
	Shared& self;
	Scene& scene;

	std::unordered_map<std::size_t, Uri<StaticMesh>> imported_meshes{};

	legsmi::Mesh const& get_mesh(std::size_t index) const {
		auto func = [index](legsmi::Mesh const& in) { return in.index == index; };
		auto it = std::ranges::find_if(self.asset_list.meshes, func);
		assert(it != self.asset_list.meshes.end());
		return *it;
	}

	Uri<> const& import_mesh(std::size_t index) {
		if (auto it = imported_meshes.find(index); it != imported_meshes.end()) { return it->second; }
		auto uri = mesh_importer.try_import(get_mesh(index));
		assert(!uri.value().empty());
		auto [it, _] = imported_meshes.insert_or_assign(index, std::move(uri));
		return it->second;
	}

	void add_node(std::size_t index, Id<Node> parent) {
		auto const& in_node = mesh_importer.root.nodes[index];
		auto& out_node = scene.spawn(NodeCreateInfo{.transform = legsmi::from(in_node.transform), .name = in_node.name, .parent = parent});
		if (in_node.mesh) {
			auto uri = import_mesh(*in_node.mesh);
			scene.get(out_node.entity).attach(std::make_unique<MeshRenderer>(StaticMeshRenderer{.mesh = std::move(uri)}));
		}
		parent = out_node.id();
		for (auto const node_index : in_node.children) { add_node(node_index, parent); }
	}

	std::vector<Uri<StaticMesh>> operator()(legsmi::Scene const& scene) {
		auto const& in_scene = mesh_importer.root.scenes[scene.index];
		for (auto const node_index : in_scene.root_nodes) { add_node(node_index, {}); }
		auto ret = std::vector<Uri<StaticMesh>>{};
		ret.reserve(imported_meshes.size());
		for (auto& [_, uri] : imported_meshes) { ret.push_back(std::move(uri)); }
		return ret;
	}
};

void GltfImportWizard::TypePage::update(Shared& out) {
	ImGui::Text("Asset to import:");
	if (ImGui::RadioButton("Mesh", type == Type::eMesh)) { type = Type::eMesh; }
	if (out.allow_scene) {
		if (ImGui::RadioButton("Scene", type == Type::eScene)) { type = Type::eScene; }
	}
}

void GltfImportWizard::MeshPage::setup(Shared const& shared) {
	auto const directory = fs::path{shared.gltf_path}.stem().string();
	entries.clear();
	for (auto const [mesh, index] : enumerate(shared.asset_list.meshes)) {
		auto& entry = entries.emplace_back();
		entry.mesh = mesh;
		entry.display_name = fmt::format("[{}] {}", index, entry.mesh.name);
		auto export_uri = entry.mesh.name;
		if (!export_uri.empty()) { export_uri += "/"; }
		entry.export_uri.set(fmt::format("{}/{}", directory, export_uri));
	}
}

void GltfImportWizard::MeshPage::update(Shared& out) {
	if (entries.empty()) { return; }
	if (selected >= entries.size()) { selected = 0; }
	auto& selected_entry = entries[selected];
	ImGui::Text("Source Mesh");
	ImGui::SetNextItemWidth(480.0f);
	if (auto w = Combo{"##Source Mesh", selected_entry.display_name.c_str()}) {
		for (auto const [entry, index] : enumerate(entries)) {
			if (ImGui::Selectable(entry.display_name.c_str(), index == selected)) { selected = index; }
		}
	}
	ImGui::Text("%s", FixedString{"Type: {}", selected_entry.mesh.mesh_type == MeshType::eSkinned ? "SkinnedMesh" : "StaticMesh"}.c_str());
	ImGui::Separator();
	ImGui::Text("Destination Directory");
	ImGui::SetNextItemWidth(480.0f);
	selected_entry.export_uri("##Destination Directory");
	auto const dir = fs::path{out.root_path} / selected_entry.export_uri.view();
	bool allow_import{true};
	if (fs::is_regular_file(dir)) {
		ImGui::TextColored({0.8f, 0.1f, 0.1f, 1.0f}, "File exists");
		allow_import = false;
	}
	if (allow_import && fs::is_directory(dir) && !fs::is_empty(dir)) { ImGui::TextColored({0.8f, 0.8f, 0.1f, 1.0f}, "Directory not empty"); }
}

Uri<Mesh> GltfImportWizard::MeshPage::import_mesh(Shared& out) {
	auto& selected_entry = entries[selected];
	auto importer = out.asset_list.mesh_importer(out.root_path, std::string{selected_entry.export_uri.view()});
	return importer.try_import(selected_entry.mesh);
}

void GltfImportWizard::ScenePage::setup(Shared const& shared) {
	auto dir = fs::path{shared.gltf_path}.stem();
	assets_dir.set(dir.string());
	auto file = dir / dir.filename();
	file += ".scene.json";
	scene_uri.set(file.generic_string());

	entries.clear();
	for (auto const [scene, index] : enumerate(shared.asset_list.scenes)) {
		auto& entry = entries.emplace_back();
		entry.scene = scene;
		entry.display_name = fmt::format("[{}] {}", index, entry.scene.name);
	}
}

void GltfImportWizard::ScenePage::update(Shared& out) {
	if (entries.empty() || out.asset_list.scenes.empty()) { return; }
	if (selected >= entries.size()) { selected = 0; }
	auto& selected_entry = entries[selected];
	ImGui::Text("Scene");
	if (auto w = Combo{"##Scene", selected_entry.display_name.c_str()}) {
		for (auto const [entry, index] : enumerate(entries)) {
			if (ImGui::Selectable(entry.display_name.c_str(), index == selected)) { selected = index; }
		}
	}
	bool allow_import{true};
	ImGui::Separator();
	ImGui::Text("Assets Directory");
	ImGui::SetNextItemWidth(480.0f);
	assets_dir("##Assets Directory");
	auto const dir = fs::path{out.root_path} / assets_dir.view();
	if (fs::is_regular_file(dir)) { allow_import = false; }
	if (!allow_import) {
		ImGui::TextColored({0.8f, 0.1f, 0.1f, 1.0f}, "Name collision with existing file");
	} else if (fs::is_directory(dir) && !fs::is_empty(dir)) {
		ImGui::TextColored({0.8f, 0.8f, 0.1f, 1.0f}, "Directory not empty");
	}
	ImGui::Text("Scene URI");
	ImGui::SetNextItemWidth(480.0f);
	scene_uri("##Scene URI");
	auto const file = fs::path{out.root_path} / scene_uri.view();
	if (allow_import && fs::is_regular_file(file)) { ImGui::TextColored({0.8f, 0.8f, 0.1f, 1.0f}, "File exists"); }
}

Uri<Scene> GltfImportWizard::ScenePage::import_scene(Shared& out) {
	auto const export_path = fs::path{out.root_path} / assets_dir.view();
	if (fs::exists(export_path)) {
		logger::warn("[Import] Deleting [{}]", assets_dir.view());
		fs::remove_all(export_path);
	}
	auto scene = Scene{};
	auto mesh_importer = out.asset_list.mesh_importer(out.root_path, std::string{assets_dir.view()});
	auto scene_importer = out.asset_list.scene_importer(out.root_path, std::string{assets_dir.view()}, std::string{scene_uri.view()});
	auto ret = Walk{std::move(mesh_importer), std::move(scene_importer), assets_dir, out, scene}(entries[selected].scene);
	auto scene_path = fs::path{out.root_path} / scene_uri.view();
	auto json = dj::Json{};
	scene.serialize(json);
	json.to_file(scene_path.string().c_str());
	return scene_uri.view();
}

GltfImportWizard::GltfImportWizard(std::string gltf_path, std::string root) {
	m_shared.root_path = std::move(root);
	m_shared.gltf_path = std::move(gltf_path);
	m_shared.gltf_filename = fs::path{m_shared.gltf_path}.filename().string();
	m_shared.asset_list = legsmi::peek_assets(m_shared.gltf_path);
	m_shared.allow_scene = std::ranges::none_of(m_shared.asset_list.meshes, [](legsmi::Mesh const& mesh) { return mesh.mesh_type == MeshType::eSkinned; });

	m_pages.mesh.setup(m_shared);
	if (m_shared.allow_scene) { m_pages.scene.setup(m_shared); }
	m_current = Page::eType;

	Modal::open("Gltf Import Wizard");
}

GltfImportWizard::Result GltfImportWizard::update() {
	auto ret = Result{};
	if (!ImGui::IsPopupOpen("Gltf Import Wizard")) {
		ret.inactive = true;
		return ret;
	}
	ImGui::SetNextWindowSize({500.0f, 400.0f}, ImGuiCond_Appearing);
	if (auto m = Modal{"Gltf Import Wizard", {true}, {true}}) {
		ImGui::Text("%s", FixedString<256>{"Importing [{}]", m_shared.gltf_filename}.c_str());
		if (auto w = Window{m, "Import", {0.0f, 280.0f}, {true}}) {
			switch (m_current) {
			case Page::eType: m_pages.type.update(m_shared); break;
			case Page::eMesh: m_pages.mesh.update(m_shared); break;
			case Page::eScene: m_pages.scene.update(m_shared); break;
			default: break;
			}
		}
		bool const mesh_or_scene = m_current == Page::eMesh || m_current == Page::eScene;
		if (mesh_or_scene) {
			if (m_current == Page::eScene) {
				ImGui::Checkbox("Load imported Scene", &m_load_scene);
			} else {
				ImGui::Checkbox("Load imported Mesh into current Scene", &m_load_mesh);
			}
		}
		ImGui::Separator();
		if (m_current == Page::eType) {
			if (ImGui::Button("Next")) {
				m_current = m_pages.type.type == Type::eScene ? Page::eScene : Page::eMesh;
				return {};
			}
		}
		if (m_current > Page::eType) {
			if (ImGui::Button("Back")) {
				m_current = Page::eType;
				return ret;
			}
		}
		if (mesh_or_scene) {
			ImGui::SameLine();
			if (m_current == Page::eScene) {
				if (ImGui::Button("Import Scene")) {
					ret.scene = m_pages.scene.import_scene(m_shared);
					ret.should_load = m_load_scene;
				}
			} else {
				if (ImGui::Button("Import Mesh")) {
					ret.mesh = m_pages.mesh.import_mesh(m_shared);
					ret.should_load = m_load_mesh;
				}
			}
		}
	}
	if (ret) { Modal::close_current(); }
	return ret;
}
} // namespace levk::imcpp
