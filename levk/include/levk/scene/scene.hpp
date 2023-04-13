#pragma once
#include <levk/graphics/lights.hpp>
#include <levk/graphics/renderer.hpp>
#include <levk/graphics/skeleton.hpp>
#include <levk/io/serializable.hpp>
#include <levk/node.hpp>
#include <levk/scene/entity.hpp>
#include <levk/scene/scene_camera.hpp>
#include <levk/ui/view.hpp>
#include <levk/util/monotonic_map.hpp>
#include <levk/util/time.hpp>
#include <levk/window/window_state.hpp>
#include <unordered_set>

namespace levk {
class Serializer;

class Scene : public Renderer, public Serializable {
  public:
	static AssetList peek_assets(Serializer const& serializer, dj::Json const& json);

	Node& spawn(Node::CreateInfo const& node_create_info = {});
	void tick(WindowState const& window_state, Time dt);
	bool destroy(Id<Entity> entity);

	Ptr<Entity const> find(Id<Entity> id) const { return m_entities.find(id); }
	Ptr<Entity> find(Id<Entity> id) { return m_entities.find(id); }
	Entity const& get(Id<Entity> id) const { return m_entities.get(id); }
	Entity& get(Id<Entity> id) { return m_entities.get(id); }

	Node::Tree const& nodes() const { return m_nodes; }
	Node::Locator node_locator() { return m_nodes; }

	bool detach_from(Entity& out_entity, TypeId::value_type component_type) const;

	bool empty() const { return m_entities.empty(); }

	virtual void setup() {}
	void render(RenderList& out) const override;

	Skeleton::Instance instantiate_skeleton(Skeleton const& source, Id<Node> root);

	std::string_view type_name() const override { return "Scene"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;

	Lights lights{};
	SceneCamera camera{};
	std::string name{};
	ui::View ui_root{};

  protected:
	virtual void tick(Time /*dt*/) {}

	WindowState const& window_state() const { return m_window_state; }
	Input const& input() const { return window_state().input; }

  private:
	Node::Tree m_nodes{};
	MonotonicMap<Entity> m_entities{};

	WindowState m_window_state{};
	std::unordered_set<Id<Entity>::id_type> m_to_destroy{};
};
} // namespace levk
