#pragma once
#include <levk/graphics/camera.hpp>
#include <levk/graphics/lights.hpp>
#include <levk/graphics/renderer.hpp>
#include <levk/io/serializable.hpp>
#include <levk/node.hpp>
#include <levk/scene/entity.hpp>
#include <levk/ui/view.hpp>
#include <levk/util/monotonic_map.hpp>
#include <levk/util/time.hpp>
#include <unordered_set>

namespace levk {
struct WindowState;
class Serializer;
struct StaticMesh;
struct SkinnedMesh;

class Scene : public Renderer, public Serializable {
  public:
	static AssetList peek_assets(Serializer const& serializer, dj::Json const& json);

	bool load_into_tree(Uri<StaticMesh> const& uri);
	bool load_into_tree(Uri<SkinnedMesh> const& uri);
	bool add_to_tree(Uri<StaticMesh> const& uri, StaticMesh const& static_mesh);
	bool add_to_tree(Uri<SkinnedMesh> const& uri, SkinnedMesh const& skinned_mesh);

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

	void render(RenderList& out) const override;

	std::string_view type_name() const override { return "Scene"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;

	Lights lights{};
	Camera camera{};
	std::string name{};
	ui::View ui_root{};

  private:
	Node::Tree m_nodes{};
	MonotonicMap<Entity> m_entities{};
	std::unordered_set<Id<Entity>::id_type> m_to_destroy{};
};
} // namespace levk
