#pragma once
#include <levk/audio/music.hpp>
#include <levk/audio/sfx.hpp>
#include <levk/graphics/lights.hpp>
#include <levk/node/node_tree.hpp>
#include <levk/scene/collision.hpp>
#include <levk/scene/entity.hpp>
#include <levk/scene/scene_camera.hpp>
#include <levk/ui/view.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/monotonic_map.hpp>
#include <levk/util/pinned.hpp>
#include <levk/util/time.hpp>
#include <levk/util/type_id.hpp>
#include <unordered_map>

namespace levk {
struct RenderList;
struct Level;

class Scene : public Pinned {
  public:
	virtual ~Scene() = default;

	Entity& spawn(NodeCreateInfo create_info);

	Ptr<Entity const> find_entity(Id<Entity> id) const;
	Ptr<Entity> find_entity(Id<Entity> id);

	Entity const& get_entity(Id<Entity> id) const;
	Entity& get_entity(Id<Entity> id);

	void destroy_entity(Id<Entity> id);

	NodeTree const& node_tree() const { return m_nodes; }
	NodeLocator node_locator() { return m_nodes; }
	glm::mat4 global_transform(Entity const& entity) const { return m_nodes.global_transform(entity.node_id()); }
	glm::mat4 global_transform(Id<Entity> id) const;

	Level export_level() const;
	bool import_level(Level const& level);

	WindowState const& window_state() const;
	Input const& input() const;

	virtual void setup() {}
	virtual void tick(Duration dt);
	virtual void render(RenderList& out) const;
	virtual void clear();

	ui::View ui_root{};
	SceneCamera camera{};
	Lights lights{};
	Collision collision{};
	Sfx sfx;
	Music music;

	glm::vec3 shadow_frustum{100.0f};

	std::string name{};

  protected:
	Entity make_entity(Id<Node> node_id);

	MonotonicMap<Entity> m_entities{};
	NodeTree m_nodes{};
	Logger m_logger{"Scene"};
};
} // namespace levk
