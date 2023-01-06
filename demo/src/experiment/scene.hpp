#pragma once
#include <experiment/entity.hpp>
#include <levk/asset/uri.hpp>
#include <levk/engine.hpp>
#include <levk/resources.hpp>
#include <levk/serializable.hpp>
#include <levk/util/reader.hpp>
#include <levk/util/time.hpp>
#include <variant>

namespace levk::experiment {
struct StaticMeshRenderer {
	Id<StaticMesh> mesh{};
	std::vector<Transform> instances{};
	asset::Uri<StaticMesh> mesh_uri{};

	void render(Entity const& entity) const;
};

struct SkeletonController : Component {
	using Animation = Skeleton::Animation;

	std::optional<Id<Animation>> enabled{};
	float time_scale{1.0f};
	Time elapsed{};

	void change_animation(std::optional<Id<Skeleton::Animation>> index);
	void tick(Time dt) override;

	std::string_view type_name() const override { return "skeleton_controller"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;
};

struct SkinnedMeshRenderer {
	Skeleton::Instance skeleton{};
	Id<SkinnedMesh> mesh{};
	asset::Uri<SkinnedMesh> mesh_uri{};

	DynArray<glm::mat4> joint_matrices{};

	void set_mesh(Id<SkinnedMesh> id, Skeleton::Instance instance);
	void render(Entity const& entity) const;
};

struct MeshRenderer : Entity::Renderer {
	std::variant<StaticMeshRenderer, SkinnedMeshRenderer> renderer{};

	MeshRenderer(std::variant<StaticMeshRenderer, SkinnedMeshRenderer> renderer = StaticMeshRenderer{}) : renderer(std::move(renderer)) {}

	void render(Entity const& entity) const override;

	std::string_view type_name() const override { return "mesh_renderer"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;
};

class Scene : public GraphicsRenderer, public ISerializable {
  public:
	struct Renderer;

	bool import_gltf(char const* in_path, std::string_view dest_dir);
	bool load_mesh_into_tree(std::string_view uri);
	bool load_into_tree(asset::Uri<StaticMesh> uri);
	bool load_into_tree(asset::Uri<SkinnedMesh> uri);
	bool add_to_tree(std::string_view uri, Id<SkinnedMesh> id);
	bool add_to_tree(std::string_view uri, Id<StaticMesh> id);

	Node& spawn(Entity entity, Node::CreateInfo const& node_create_info = {});
	void tick(Time dt);

	Ptr<Entity const> find(Id<Entity> id) const { return m_entities.find(id); }
	Ptr<Entity> find(Id<Entity> id) { return m_entities.find(id); }
	Entity const& get(Id<Entity> id) const { return m_entities.get(id); }
	Entity& get(Id<Entity> id) { return m_entities.get(id); }

	Node::Tree const& nodes() const { return m_nodes; }
	Node::Locator node_locator() { return m_nodes; }

	bool from_json(char const* path);

	void render_3d() final;
	void render_ui() final {}

	std::string_view type_name() const override { return "scene"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;

	std::string name{};

  private:
	Node::Tree m_nodes{};
	MonotonicMap<Entity> m_entities{};
	std::vector<Ptr<Entity>> m_sorted{};
};
} // namespace levk::experiment
