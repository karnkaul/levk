#pragma once
#include <levk/engine.hpp>
#include <levk/mesh_resources.hpp>
#include <levk/scene_tree.hpp>
#include <levk/util/reader.hpp>
#include <levk/util/time.hpp>
#include <variant>

namespace levk::experiment {
struct StaticMeshRenderer {
	Id<StaticMesh> mesh{};
	Id<Node> node{};
	std::vector<Transform> instances{};

	void render(GraphicsDevice& device, MeshResources const& resources, Node::Tree const& tree) const;
};

struct SkinnedMeshRenderer {
	struct Skin {
		std::optional<Id<Animation>> enabled{};
		Skeleton::Instance skeleton{};

		void change_animation(std::optional<Id<Animation>> index);
	};

	Id<SkinnedMesh> mesh{};
	Skin skin{};

	void tick(Node::Tree& tree, Time dt);
	void render(GraphicsDevice& device, MeshResources const& resources, Node::Tree const& tree) const;
};

struct MeshRenderer {
	Ptr<GraphicsDevice> device;
	Ptr<MeshResources const> resources;
	std::variant<StaticMeshRenderer, SkinnedMeshRenderer> renderer{};

	void tick(Node::Tree& tree, Time dt);
	void render(Node::Tree const& tree) const;
};

struct ImportResult {
	std::vector<Id<Texture>> added_textures{};
	std::vector<Id<Material>> added_materials{};
	std::vector<Id<Skeleton>> added_skeletons{};
	std::vector<Id<StaticMesh>> added_static_meshes{};
	std::vector<Id<SkinnedMesh>> added_skinned_meshes{};
};

class Scene {
  public:
	struct Renderer;

	Scene(Engine& engine, MeshResources& resources) : engine(&engine), mesh_resources(&resources) {}

	ImportResult import_gltf(char const* in_path, char const* out_path);

	void tick(Time dt);

	SceneTree tree{};
	Ptr<Engine> engine;
	Ptr<MeshResources> mesh_resources;

	Id<Node> test_node{};
};

struct Scene::Renderer : GraphicsRenderer {
	Ptr<Scene const> scene{};

	Renderer(Scene const& scene) : scene(&scene) {}

	void render_3d() final;
	void render_ui() final {}
};
} // namespace levk::experiment
