#pragma once
#include <levk/scene/render_component.hpp>

namespace levk {
class StaticMeshRenderer : public RenderComponent {
  public:
	std::string_view type_name() const override { return "StaticMeshRenderer"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;
	void inspect(imcpp::OpenWindow) override;
	void render(DrawList& out) const final;
	void add_assets(AssetList& out, dj::Json const& json) const override;

	std::vector<Transform> instances{};
	Uri<StaticMesh> mesh{};
};
} // namespace levk
