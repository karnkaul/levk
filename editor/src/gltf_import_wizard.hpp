#pragma once
#include <legsmi/legsmi.hpp>
#include <levk/asset/asset_type.hpp>
#include <levk/imcpp/common.hpp>
#include <levk/imcpp/input_text.hpp>

namespace levk {
struct Level;
}

namespace levk::imcpp {
class GltfImportWizard {
  public:
	enum class Type { eMesh, eLevel };

	struct Result {
		Uri<> uri{};
		asset::Type type{};
		bool should_load{};
		bool inactive{};

		explicit operator bool() const { return !!uri; }
	};

	GltfImportWizard(std::string gltf_path, std::string root);

	Result update();

  private:
	enum class Page { eNone, eType, eMesh, eLevel };

	struct Shared {
		std::string gltf_path{};
		std::string gltf_filename{};
		std::string root_path{};
		legsmi::AssetList asset_list{};
		bool allow_level{};
	};

	struct TypePage {
		Type type{};

		void update(Shared& out);
	};

	struct MeshPage {
		struct Entry {
			std::string display_name{};
			legsmi::Mesh mesh{};
			InputText<256> export_uri{};
		};

		std::vector<Entry> entries{};
		std::size_t selected{};

		void setup(Shared const& shared);
		void update(Shared& out);
		Uri<Mesh> import_mesh(Shared& out);
	};

	struct LevelPage {
		struct Entry {
			std::string display_name{};
			legsmi::Scene scene{};
			InputText<256> export_uri{};
		};

		std::vector<Entry> entries{};
		std::size_t selected{};
		InputText<256> assets_dir{};

		void setup(Shared const& shared);
		void update(Shared& out);
		Uri<Level> import_scene(Shared& out);
	};

	struct Walk;

	struct {
		TypePage type{};
		MeshPage mesh{};
		LevelPage level{};
	} m_pages{};

	Shared m_shared{};
	Page m_current{};
	bool m_load_mesh{};
	bool m_load_level{};
};
} // namespace levk::imcpp
