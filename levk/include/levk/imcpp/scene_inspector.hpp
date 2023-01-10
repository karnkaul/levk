#pragma once
#include <levk/imcpp/common.hpp>
#include <levk/scene.hpp>
#include <levk/util/path_tree.hpp>

namespace levk::imcpp {
class SceneInspector {
  public:
	struct Inspect {
		enum class Type { eEntity, eCamera, eLights };

		Id<Entity> entity{};
		Type type{};

		explicit constexpr operator bool() const { return type != Type::eEntity || entity != Id<Entity>{}; }

		constexpr bool operator==(Id<Entity> id) const { return type == Type::eEntity && id == entity; }
		constexpr bool operator==(Type desired) const { return type == desired; }
	};

	Inspect inspect(NotClosed<Window> w, Scene& scene);

  private:
	bool check_stale();
	void camera_node();
	bool walk_node(Node& node);
	void draw_scene_tree(NotClosed<Window> w);
	void draw_inspector(NotClosed<Window> w);

	Inspect m_inspecting{};
	Ptr<Scene> m_scene{};
	void const* m_prev{};
};
} // namespace levk::imcpp
