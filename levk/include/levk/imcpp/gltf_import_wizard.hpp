#pragma once
#include <levk/asset/gltf_importer.hpp>
#include <levk/imcpp/common.hpp>
#include <levk/imcpp/input_text.hpp>

namespace levk {
class Scene;
}

namespace levk::imcpp {
class GltfImportWizard {
  public:
	enum class Type { eMesh, eScene };

	struct Result {
		Uri<Scene> scene{};
		std::optional<std::variant<Uri<StaticMesh>, Uri<SkinnedMesh>>> mesh{};
		bool should_load{};
		bool inactive{};

		explicit operator bool() const { return scene || mesh; }
	};

	GltfImportWizard(std::string gltf_path, std::string root);

	Result update();

  private:
	enum class Page { eNone, eType, eMesh, eScene };

	struct Shared {
		std::string gltf_path{};
		std::string gltf_filename{};
		std::string root_path{};
		asset::GltfImporter::List asset_list{};
		bool allow_scene{};
	};

	struct TypePage {
		Type type{};

		void update(Shared& out);
	};

	struct MeshPage {
		struct Entry {
			std::string display_name{};
			asset::GltfMesh mesh{};
			InputText<256> export_uri{};
		};

		std::vector<Entry> entries{};
		std::size_t selected{};

		void setup(Shared const& shared);
		void update(Shared& out);
		std::variant<Uri<StaticMesh>, Uri<SkinnedMesh>> import_mesh(Shared& out);
	};

	struct ScenePage {
		struct Entry {
			std::string display_name{};
			asset::GltfScene scene{};
		};

		std::vector<Entry> entries{};
		std::size_t selected{};
		InputText<256> assets_dir{};
		InputText<256> scene_uri{};

		void setup(Shared const& shared);
		void update(Shared& out);
		Uri<Scene> import_scene(Shared& out);
	};

	struct Walk;

	struct {
		TypePage type{};
		MeshPage mesh{};
		ScenePage scene{};
	} m_pages{};

	Shared m_shared{};
	Page m_current{};
	bool m_load_mesh{};
	bool m_load_scene{};
};
} // namespace levk::imcpp
