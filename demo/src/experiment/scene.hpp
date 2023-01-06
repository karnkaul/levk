#pragma once
#include <experiment/component.hpp>
#include <experiment/entity.hpp>
#include <levk/engine.hpp>
#include <levk/render_resources.hpp>
#include <levk/util/reader.hpp>
#include <levk/util/time.hpp>
#include <variant>

namespace levk::experiment {
struct StaticMeshRenderer {
	Id<StaticMesh> mesh{};
	std::vector<Transform> instances{};

	void render(Entity const& entity) const;
};

struct SkeletonController : TickComponent {
	using Animation = Skeleton::Animation;

	std::optional<Id<Animation>> enabled{};
	float time_scale{1.0f};
	Time elapsed{};

	void change_animation(std::optional<Id<Skeleton::Animation>> index);
	void tick(Time dt) override;
};

struct SkinnedMeshRenderer {
	Skeleton::Instance skeleton{};
	Id<SkinnedMesh> mesh{};

	DynArray<glm::mat4> joint_matrices{};

	void set_mesh(Id<SkinnedMesh> id, Skeleton::Instance instance);
	void render(Entity const& entity) const;
};

struct MeshRenderer : Entity::Renderer {
	std::variant<StaticMeshRenderer, SkinnedMeshRenderer> renderer{};

	MeshRenderer(std::variant<StaticMeshRenderer, SkinnedMeshRenderer> renderer) : renderer(std::move(renderer)) {}

	void render(Entity const& entity) const override;
};

class Scene {
  public:
	struct Renderer;

	bool import_gltf(char const* in_path, char const* out_path);
	bool load_mesh_into_tree(char const* path);
	bool add_mesh_to_tree(Id<SkinnedMesh> id);
	bool add_mesh_to_tree(Id<StaticMesh> id);

	Node& spawn(Entity entity, Node::CreateInfo const& node_create_info = {});
	void tick(Time dt);

	Ptr<Entity const> find(Id<Entity> id) const { return m_entities.find(id); }
	Ptr<Entity> find(Id<Entity> id) { return m_entities.find(id); }
	Entity const& get(Id<Entity> id) const { return m_entities.get(id); }
	Entity& get(Id<Entity> id) { return m_entities.get(id); }

	Node::Tree const& nodes() const { return m_nodes; }
	Node::Locator node_locator() { return m_nodes; }

  private:
	Node::Tree m_nodes{};
	MonotonicMap<Entity> m_entities{};
	std::vector<Ptr<Entity>> m_sorted{};
};

struct Scene::Renderer : GraphicsRenderer {
	Ptr<Scene const> scene{};

	Renderer(Scene const& scene) : scene(&scene) {}

	void render_3d() final;
	void render_ui() final {}
};
} // namespace levk::experiment
