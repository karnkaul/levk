#pragma once
#include <levk/context.hpp>
#include <levk/imcpp/asset_inspector.hpp>
#include <levk/imcpp/engine_status.hpp>
#include <levk/imcpp/gltf_import_wizard.hpp>
#include <levk/imcpp/log_display.hpp>
#include <levk/imcpp/scene_graph.hpp>
#include <levk/runtime.hpp>
#include <levk/scene.hpp>
#include <levk/util/async_task.hpp>
#include <levk/vfs/disk_vfs.hpp>
#include <main_menu.hpp>

#include <levk/font/font.hpp>
#include <levk/font/static_font_atlas.hpp>

namespace levk {
struct FreeCam {
	Ptr<Window> window{};
	glm::vec3 move_speed{10.0f};
	float look_speed{0.3f};

	glm::vec2 prev_cursor{};
	glm::vec2 pitch_yaw{};

	void tick(Transform& transform, Input const& input, Time dt);
};

class Editor : public Runtime {
  public:
	Editor(std::unique_ptr<DiskVfs> vfs);
	Editor(std::string data_path) : Editor(std::make_unique<DiskVfs>(std::move(data_path))) {}

  private:
	void tick(Frame const& frame) override;
	void render() override;

	void save_scene() const;

	FreeCam m_free_cam{};
	imcpp::AssetInspector m_asset_inspector{};
	imcpp::SceneGraph m_scene_graph{};
	imcpp::EngineStatus m_engine_status{};
	imcpp::LogDisplay m_log_display{};
	std::optional<imcpp::GltfImportWizard> m_gltf_importer{};
	MainMenu m_main_menu{};
	std::unique_ptr<DiskVfs> m_vfs{};

	Uri<Scene> m_scene_uri{};
	std::unique_ptr<AsyncTask<Uri<>>> m_load{};

	struct {
		std::optional<StaticFontAtlas> atlas{};
		std::optional<Font> font{};
		std::optional<MeshGeometry> mesh{};
		std::optional<Material> material{};
	} m_test{};
};
} // namespace levk
