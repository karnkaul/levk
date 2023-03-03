#pragma once
#include <levk/graphics_common.hpp>
#include <levk/transform.hpp>

namespace levk {
class MeshGeometry;
class Material;
struct StaticMesh;
struct SkinnedMesh;
class AssetProviders;

class RenderPass {
  public:
	virtual ~RenderPass() = default;

	RenderPass(AssetProviders const& asset_providers) : m_providers(asset_providers) {}

	virtual void render(StaticMesh const& mesh, std::span<Transform const> instances, glm::mat4 const& parent = matrix_identity_v) const = 0;
	virtual void render(SkinnedMesh const& mesh, std::span<glm::mat4 const> joints) const = 0;
	virtual void draw(MeshGeometry const& geometry, Material const& material, glm::mat4 const& transform, Topology topology) const = 0;

  protected:
	AssetProviders const& m_providers;
};
} // namespace levk
