#pragma once
#include <levk/graphics/skeleton.hpp>
#include <levk/scene/render_component.hpp>

namespace levk {
class SkinnedMeshRenderer : public RenderComponent {
  public:
	void set_mesh(Uri<SkinnedMesh> uri, Skeleton::Instance skeleton);

	Skeleton::Instance const& skeleton() const { return m_skeleton; }

	std::string_view type_name() const override { return "SkinnedMeshRenderer"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;
	void inspect(imcpp::OpenWindow) override;
	void render(RenderList& out) const final;
	void add_assets(AssetList& out, dj::Json const& json) const override;

  protected:
	Skeleton::Instance m_skeleton{};
	Uri<SkinnedMesh> m_mesh{};

	DynArray<glm::mat4> m_joint_matrices{};
};
} // namespace levk
