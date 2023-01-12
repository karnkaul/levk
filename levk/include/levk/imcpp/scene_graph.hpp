#pragma once
#include <levk/imcpp/input_text.hpp>
#include <levk/imcpp/inspector.hpp>
#include <levk/util/path_tree.hpp>

namespace levk::imcpp {
class SceneGraph {
  public:
	Inspector::Target draw_to(NotClosed<Window> w, Scene& scene);

	Inspector const& inspector() const { return m_inspector; }
	Inspector& inspector() { return m_inspector; }

  private:
	bool check_stale();
	void camera_node();
	bool walk_node(Node& node);
	void draw_scene_tree(NotClosed<Window> w);

	Inspector m_inspector{};
	Ptr<Scene> m_scene{};
	void const* m_prev{};

	bool m_right_clicked{};
	Id<Entity> m_right_clicked_entity{};
	InputText<> m_entity_name{};
};
} // namespace levk::imcpp
