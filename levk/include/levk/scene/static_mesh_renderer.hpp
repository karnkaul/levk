#pragma once
#include <levk/scene/component.hpp>
#include <levk/transform.hpp>
#include <levk/uri.hpp>

namespace levk {
struct StaticMesh;

class StaticMeshRenderer : public RenderComponent {
  public:
	void render(DrawList& out) const final;

	std::vector<Transform> instances{};
	Uri<StaticMesh> mesh_uri{};
};
} // namespace levk
