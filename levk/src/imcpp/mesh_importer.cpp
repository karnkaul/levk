#include <imgui.h>
#include <levk/imcpp/mesh_importer.hpp>
#include <levk/util/enumerate.hpp>
#include <levk/util/fixed_string.hpp>
#include <filesystem>

namespace levk::imcpp {
namespace fs = std::filesystem;

void MeshImporter::setup(std::string path, std::string root) {
	m_root = std::move(root);
	auto gltf_path = fs::path{path}.filename();
	m_gltf_path = gltf_path.generic_string();
	auto directory = gltf_path.stem().string();
	m_asset_list = asset::GltfImporter::peek(path);
	setup_entries(directory);
	Modal::open("Mesh Importer");
}

void MeshImporter::setup_entries(std::string_view const directory) {
	m_entries.clear();
	for (auto const [mesh, index] : enumerate(m_asset_list.meshes)) {
		auto& entry = m_entries.emplace_back();
		entry.mesh = mesh;
		entry.display_name = fmt::format("[{}] {}", index, entry.mesh.name);
		auto export_uri = entry.mesh.name;
		if (!export_uri.empty()) { export_uri += "/"; }
		entry.export_uri.set(fmt::format("{}/{}", directory, export_uri));
		entry.type = mesh.mesh_type;
	}
}

Uri MeshImporter::update() {
	if (m_entries.empty()) { return {}; }
	if (auto m = Modal{"Mesh Importer", {true}, {true}, ImGuiWindowFlags_AlwaysAutoResize}) {
		ImGui::Text("%s", FixedString<256>{"Importing [{}]", m_gltf_path}.c_str());
		ImGui::Separator();
		if (m_selected >= m_entries.size()) { m_selected = 0; }
		auto& selected = m_entries[m_selected];
		ImGui::Text("Source Mesh");
		if (auto w = Combo{"##Source Mesh", selected.display_name.c_str()}) {
			for (auto const [entry, index] : enumerate(m_entries)) {
				if (ImGui::Selectable(entry.display_name.c_str(), index == m_selected)) { m_selected = index; }
			}
		}
		ImGui::Text("%s", FixedString{"Type: {}", selected.type == MeshType::eSkinned ? "SkinnedMesh" : "StaticMesh"}.c_str());
		ImGui::Separator();
		ImGui::Text("Destination Directory");
		selected.export_uri("##Destination Directory");
		auto const dir = fs::path{m_root} / selected.export_uri.view();
		bool allow_import{true};
		if (fs::is_regular_file(dir)) {
			ImGui::TextColored({0.8f, 0.1f, 0.1f, 1.0f}, "File exists");
			allow_import = false;
		}
		if (allow_import && fs::is_directory(dir) && !fs::is_empty(dir)) { ImGui::TextColored({0.8f, 0.8f, 0.1f, 1.0f}, "Directory not empty"); }

		ImGui::Separator();
		if (ImGui::Button("Import") && allow_import) {
			m.close_current();
			auto export_path = (fs::path{m_root} / selected.export_uri.view()).string();
			auto importer = m_asset_list.importer(export_path);
			auto ret = importer.import_mesh(selected.mesh);
			return (fs::path{selected.export_uri.view()} / ret.value()).generic_string();
		}
	}
	return {};
}
} // namespace levk::imcpp
