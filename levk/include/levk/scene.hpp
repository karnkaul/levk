#pragma once
#include <levk/engine.hpp>
#include <levk/entity.hpp>
#include <levk/resources.hpp>
#include <levk/serializable.hpp>
#include <levk/uri.hpp>
#include <levk/util/monotonic_map.hpp>
#include <levk/util/reader.hpp>
#include <levk/util/time.hpp>
#include <unordered_set>
#include <variant>

namespace levk {
struct StaticMeshRenderer {
	std::vector<Transform> instances{};
	Uri<StaticMesh> uri{};

	void render(Entity const& entity) const;
};

struct SkeletonController : Component {
	using Animation = Skeleton::Animation;

	std::optional<Id<Animation>> enabled{};
	float time_scale{1.0f};
	Time elapsed{};

	void change_animation(std::optional<Id<Skeleton::Animation>> index);
	void tick(Time dt) override;

	std::string_view type_name() const override { return "SkeletonController"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;
	void inspect(imcpp::OpenWindow) override;
};

struct SkinnedMeshRenderer {
	Skeleton::Instance skeleton{};
	Uri<SkinnedMesh> uri{};

	DynArray<glm::mat4> joint_matrices{};

	void set_mesh(Uri<SkinnedMesh> uri, Skeleton::Instance skeleton);
	void render(Entity const& entity) const;
};

struct MeshRenderer : RenderComponent {
	std::variant<StaticMeshRenderer, SkinnedMeshRenderer> renderer{};

	MeshRenderer(std::variant<StaticMeshRenderer, SkinnedMeshRenderer> renderer = StaticMeshRenderer{}) : renderer(std::move(renderer)) {}

	void render() const override;

	std::string_view type_name() const override { return "MeshRenderer"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;
	void inspect(imcpp::OpenWindow) override;
	void add_assets(AssetList& out, dj::Json const& json) const override;
};

class Scene : public GraphicsRenderer, public Serializable {
  public:
	struct Renderer;

	static AssetList peek_assets(dj::Json const& json);

	bool load_into_tree(Uri<StaticMesh> const& uri);
	bool load_into_tree(Uri<SkinnedMesh> const& uri);
	bool add_to_tree(Uri<StaticMesh> const& uri, StaticMesh const& static_mesh);
	bool add_to_tree(Uri<SkinnedMesh> const& uri, SkinnedMesh const& skinned_mesh);

	Node& spawn(Node::CreateInfo const& node_create_info = {});
	void tick(Time dt);
	bool destroy(Id<Entity> entity);

	Ptr<Entity const> find(Id<Entity> id) const { return m_entities.find(id); }
	Ptr<Entity> find(Id<Entity> id) { return m_entities.find(id); }
	Entity const& get(Id<Entity> id) const { return m_entities.get(id); }
	Entity& get(Id<Entity> id) { return m_entities.get(id); }

	Node::Tree const& nodes() const { return m_nodes; }
	Node::Locator node_locator() { return m_nodes; }

	bool detach_from(Entity& out_entity, TypeId::value_type component_type) const;

	bool empty() const { return m_entities.empty(); }

	void render_3d() const final;
	void render_ui() const final {}

	std::string_view type_name() const override { return "scene"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;

	std::string name{};
	Camera camera{};
	Lights lights{};

  private:
	Node::Tree m_nodes{};
	MonotonicMap<Entity> m_entities{};
	std::vector<std::reference_wrapper<Entity>> m_entity_refs{};
	mutable std::vector<std::reference_wrapper<Entity const>> m_const_entity_refs{};
	std::unordered_set<Id<Entity>::id_type> m_to_destroy{};
};
} // namespace levk
