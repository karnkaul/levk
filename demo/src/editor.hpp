#pragma once
#include <levk/context.hpp>
#include <levk/imcpp/engine_status.hpp>
#include <levk/imcpp/logger.hpp>
#include <levk/imcpp/mesh_importer.hpp>
#include <levk/imcpp/resource_list.hpp>
#include <levk/imcpp/scene_graph.hpp>
#include <levk/runtime.hpp>
#include <levk/scene.hpp>
#include <levk/util/reader.hpp>
#include <main_menu.hpp>

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
	Editor(std::string data_path);

	void save_scene() const;

	std::string data_path{};

	Scene scene{};
	FreeCam free_cam{};
	imcpp::ResourceList resource_list{};
	imcpp::SceneGraph scene_graph{};
	imcpp::EngineStatus engine_status{};
	imcpp::Logger logger{};
	imcpp::MeshImporter mesh_importer{};
	MainMenu main_menu{};

  private:
	void tick(Frame const& frame) override;
	void render() override;
};
} // namespace levk
