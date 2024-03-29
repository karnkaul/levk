#include <imgui.h>
#include <gltf_import_wizard.hpp>
#include <levk/scene/scene.hpp>
#include <levk/service.hpp>
#include <levk/util/enumerate.hpp>
#include <levk/util/fixed_string.hpp>
#include <filesystem>
#include <ranges>

namespace levk::imcpp {
namespace fs = std::filesystem;

void GltfImportWizard::TypePage::update(Shared& out) {
	ImGui::Text("Asset to import:");
	if (ImGui::RadioButton("Mesh", type == Type::eMesh)) { type = Type::eMesh; }
	if (out.allow_level) {
		if (ImGui::RadioButton("Level", type == Type::eLevel)) { type = Type::eLevel; }
	}
}

void GltfImportWizard::MeshPage::setup(Shared const& shared) {
	entries.clear();
	for (auto const [mesh, index] : enumerate(shared.asset_list.meshes)) {
		auto& entry = entries.emplace_back();
		entry.mesh = mesh;
		entry.display_name = fmt::format("[{}] {}", index, entry.mesh.name);
		entry.export_uri.set(shared.asset_list.default_dir_uri);
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
	auto imported = legsmi::ImportMap{};
	return importer.try_import(selected_entry.mesh, imported);
}

void GltfImportWizard::LevelPage::setup(Shared const& shared) {
	assets_dir.set(fs::path{shared.gltf_path}.stem().string());
	entries.clear();
	for (auto const [scene, index] : enumerate(shared.asset_list.scenes)) {
		auto& entry = entries.emplace_back();
		entry.scene = scene;
		entry.display_name = fmt::format("[{}] {}", index, entry.scene.name);
		entry.export_uri.set(shared.asset_list.make_default_level_uri(index));
	}
}

void GltfImportWizard::LevelPage::update(Shared& out) {
	if (entries.empty() || out.asset_list.scenes.empty()) { return; }
	if (selected >= entries.size()) { selected = 0; }
	auto& selected_entry = entries[selected];
	ImGui::Text("Level");
	if (auto w = Combo{"##Level", selected_entry.display_name.c_str()}) {
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
	ImGui::Text("Level URI");
	ImGui::SetNextItemWidth(480.0f);
	selected_entry.export_uri("##Level URI");
	auto const file = fs::path{out.root_path} / selected_entry.export_uri.view();
	if (allow_import && fs::is_regular_file(file)) { ImGui::TextColored({0.8f, 0.8f, 0.1f, 1.0f}, "File exists"); }
}

Uri<Level> GltfImportWizard::LevelPage::import_scene(Shared& out) {
	auto scene_importer = out.asset_list.scene_importer(out.root_path, std::string{assets_dir.view()}, out.asset_list.make_default_level_uri(selected));
	auto imported = legsmi::ImportMap{};
	return scene_importer.try_import(entries[selected].scene, imported);
}

GltfImportWizard::GltfImportWizard(std::string gltf_path, std::string root) {
	m_shared.root_path = std::move(root);
	m_shared.gltf_path = std::move(gltf_path);
	m_shared.gltf_filename = fs::path{m_shared.gltf_path}.filename().string();
	m_shared.asset_list = legsmi::peek_assets(m_shared.gltf_path, &Service<Serializer>::locate());
	m_shared.allow_level = std::ranges::none_of(m_shared.asset_list.meshes, [](legsmi::Mesh const& mesh) { return mesh.mesh_type == MeshType::eSkinned; });

	m_pages.mesh.setup(m_shared);
	if (m_shared.allow_level) { m_pages.level.setup(m_shared); }
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
			case Page::eLevel: m_pages.level.update(m_shared); break;
			default: break;
			}
		}
		bool const mesh_or_level = m_current == Page::eMesh || m_current == Page::eLevel;
		if (mesh_or_level) {
			if (m_current == Page::eLevel) {
				ImGui::Checkbox("Load imported Level", &m_load_level);
			} else {
				ImGui::Checkbox("Load imported Mesh into current Scene", &m_load_mesh);
			}
		}
		ImGui::Separator();
		if (m_current == Page::eType) {
			if (ImGui::Button("Next")) {
				m_current = m_pages.type.type == Type::eLevel ? Page::eLevel : Page::eMesh;
				return {};
			}
		}
		if (m_current > Page::eType) {
			if (ImGui::Button("Back")) {
				m_current = Page::eType;
				return ret;
			}
		}
		if (mesh_or_level) {
			ImGui::SameLine();
			if (m_current == Page::eLevel) {
				if (ImGui::Button("Import Level")) {
					ret.uri = m_pages.level.import_scene(m_shared);
					ret.type = asset::Type::eLevel;
					ret.should_load = m_load_level;
				}
			} else {
				if (ImGui::Button("Import Mesh")) {
					ret.uri = m_pages.mesh.import_mesh(m_shared);
					ret.type = asset::Type::eMesh;
					ret.should_load = m_load_mesh;
				}
			}
		}
	}
	if (ret) { Modal::close_current(); }
	return ret;
}
} // namespace levk::imcpp
