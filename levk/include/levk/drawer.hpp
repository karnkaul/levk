#pragma once
#include <levk/graphics_common.hpp>
#include <levk/transform.hpp>

namespace levk {
class MeshGeometry;
class Material;
struct StaticMesh;
struct SkinnedMesh;
class AssetProviders;

struct InstancedDraw {
	glm::mat4 const& parent;
	std::span<Transform const> instances{};
};

class Drawer {
  public:
	virtual ~Drawer() = default;

	Drawer(AssetProviders const& asset_providers) : m_providers(asset_providers) {}

	virtual void draw(StaticMesh const& mesh, std::span<Transform const> instances, glm::mat4 const& parent = matrix_identity_v) const = 0;
	virtual void draw(SkinnedMesh const& mesh, std::span<glm::mat4 const> joints) const = 0;
	virtual void draw(MeshGeometry const& geometry, Material const& material, InstancedDraw id, Topology = Topology::eTriangleList) const = 0;

	void draw(MeshGeometry const& geometry, Material const& material, Transform const& transform, Topology topology = Topology::eTriangleList) const {
		draw(geometry, material, {glm::mat4{1.0f}, {&transform, 1}}, topology);
	}

  protected:
	AssetProviders const& m_providers;
};
} // namespace levk
