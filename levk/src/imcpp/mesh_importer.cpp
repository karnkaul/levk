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
	auto asset_list = asset::GltfImporter::peek(path);
	m_importer = asset_list.importer();
	setup_entries(asset_list, directory);
	Modal::open("Mesh Importer");
}

void MeshImporter::setup_entries(AssetList const& asset_list, std::string_view const directory) {
	m_static.clear();
	m_skinned.clear();
	auto add = [directory](auto const& src, auto& dst) {
		for (auto const [asset, index] : enumerate(src)) {
			auto& entry = dst.emplace_back();
			entry.asset = asset;
			auto display_name = std::string_view{entry.asset.asset_name};
			display_name = display_name.substr(0, display_name.find(".mesh"));
			entry.filename.set(fmt::format("{}/{}", directory, display_name));
			entry.display_name = fmt::format("[{}] {}", index, display_name);
		}
	};
	add(asset_list.static_meshes, m_static);
	add(asset_list.skinned_meshes, m_skinned);
	if (m_static.empty()) { m_selected.skinned = {true}; }
}

Uri MeshImporter::update() {
	if (!m_importer) { return {}; }
	if (auto m = Modal{"Mesh Importer", {true}, {true}, ImGuiWindowFlags_AlwaysAutoResize}) {
		ImGui::Text("%s", FixedString<256>{"Importing [{}]", m_gltf_path}.c_str());
		ImGui::Separator();
		auto& source = m_selected.skinned ? m_skinned : m_static;
		assert(!source.empty());
		if (m_selected.index >= source.size()) { m_selected.index = 0; }
		auto& selected = source[m_selected.index];
		ImGui::Text("Source Mesh");
		if (auto w = Combo{"##Source Mesh", selected.display_name.c_str()}) {
			for (auto const [entry, index] : enumerate(source)) {
				if (ImGui::Selectable(entry.display_name.c_str(), index == m_selected.index)) { m_selected.index = index; }
			}
		}
		ImGui::Separator();
		ImGui::Text("Destination Directory");
		selected.filename("##Destination Directory");
		auto const dir = fs::path{m_root} / selected.filename.view();
		if (fs::is_directory(dir) && !fs::is_empty(dir)) { ImGui::TextColored({0.8f, 0.8f, 0.1f, 1.0f}, "Directory not empty"); }

		ImGui::Separator();
		if (ImGui::Button("Import")) {
			m.close_current();
			auto export_path = (fs::path{m_root} / selected.filename.view()).string();
			auto ret = m_importer.import_mesh(selected.asset, export_path);
			return (fs::path{selected.filename.view()} / ret.value()).generic_string();
		}
	}
	return {};
}
} // namespace levk::imcpp
