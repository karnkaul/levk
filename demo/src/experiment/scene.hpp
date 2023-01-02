#pragma once
#include <experiment/component.hpp>
#include <experiment/entity.hpp>
#include <levk/engine.hpp>
#include <levk/mesh_resources.hpp>
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
	std::optional<Id<Animation>> enabled{};

	void change_animation(std::optional<Id<Animation>> index);
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

	MonotonicMap<Entity> entities{};
	Node::Tree nodes{};

	Id<Node> test_node{};
	std::vector<Ptr<Entity>> sorted{};
};

struct Scene::Renderer : GraphicsRenderer {
	Ptr<Scene const> scene{};

	Renderer(Scene const& scene) : scene(&scene) {}

	void render_3d() final;
	void render_ui() final {}
};
} // namespace levk::experiment
