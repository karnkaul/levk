#pragma once
#include <levk/imcpp/input_text.hpp>
#include <levk/imcpp/scene_inspector.hpp>
#include <levk/util/path_tree.hpp>

namespace levk::imcpp {
class SceneGraph {
  public:
	SceneInspector::Target draw_to(NotClosed<Window> w, Scene& scene);

	SceneInspector const& inspector() const { return m_inspector; }
	SceneInspector& inspector() { return m_inspector; }

  private:
	bool check_stale();
	void camera_node();
	bool walk_node(Node& node);
	void draw_scene_tree(NotClosed<Window> w);
	void handle_popups();

	SceneInspector m_inspector{};
	Ptr<Scene> m_scene{};
	void const* m_prev{};

	bool m_right_clicked{};
	SceneInspector::Target m_right_clicked_target{};
	InputText<> m_entity_name{};
};
} // namespace levk::imcpp
