#pragma once
#include <levk/asset/gltf_importer.hpp>
#include <levk/imcpp/common.hpp>
#include <levk/imcpp/input_text.hpp>

namespace levk::imcpp {
class MeshImporter {
  public:
	using AssetList = asset::GltfImporter::List;

	void setup(std::string gltf_path, std::string root);

	Uri update();

  private:
	struct Entry {
		std::string display_name{};
		asset::GltfAsset asset{};
		InputText<256> export_uri{};
		bool is_skinned{};
	};

	std::string m_gltf_path{};
	asset::GltfImporter m_importer{};
	std::vector<Entry> m_entries{};
	std::size_t m_selected{};
	std::string m_root{};

	InputText<128> m_rename_buffer{};
	Ptr<Entry> m_rename_entry{};

	void setup_entries(AssetList const& list, std::string_view directory);
};
} // namespace levk::imcpp
