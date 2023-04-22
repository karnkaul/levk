#pragma once
#include <levk/graphics/skeleton.hpp>
#include <levk/scene/component.hpp>
#include <levk/util/dyn_array.hpp>

namespace levk {
struct SkinnedMesh;

class SkinnedMeshRenderer : public RenderComponent {
  public:
	void set_mesh_uri(Uri<SkinnedMesh> uri);
	Uri<SkinnedMesh> const& mesh_uri() const { return m_mesh; }

	NodeLocator joint_locator() { return {m_skeleton.joint_tree}; }
	Skeleton const& skeleton() const { return m_skeleton; }
	glm::mat4 global_transform(Id<Node> node_id) const;

	void render(DrawList& out) const final;
	std::unique_ptr<Attachment> to_attachment() const final;

	std::vector<Transform> instances{};

  protected:
	Skeleton m_skeleton{};
	Uri<SkinnedMesh> m_mesh{};

	DynArray<glm::mat4> m_joint_matrices{};
};
} // namespace levk
