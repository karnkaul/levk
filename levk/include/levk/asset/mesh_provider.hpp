#pragma once
#include <levk/asset/material_provider.hpp>
#include <levk/graphics/mesh.hpp>
#include <levk/graphics/skeleton.hpp>

namespace levk {
class SkeletonProvider;

template <typename Type>
class MeshProviderCommon : public GraphicsAssetProvider<Type> {
  public:
	MeshProviderCommon(NotNull<MaterialProvider*> material_provider)
		: GraphicsAssetProvider<Type>(&material_provider->render_device(), &material_provider->data_source()), m_material_provider(material_provider) {}

	MaterialProvider& material_provider() const { return *m_material_provider; }

  private:
	NotNull<MaterialProvider*> m_material_provider;
};

class StaticMeshProvider : public MeshProviderCommon<StaticMesh> {
  public:
	using MeshProviderCommon<StaticMesh>::MeshProviderCommon;

  private:
	Payload load_payload(Uri<StaticMesh> const& uri, Stopwatch const& stopwatch) const override;
};

class SkinnedMeshProvider : public MeshProviderCommon<SkinnedMesh> {
  public:
	SkinnedMeshProvider(NotNull<SkeletonProvider*> skeleton_provider, NotNull<MaterialProvider*> material_provider);

	SkeletonProvider& skeleton_provider() const { return *m_skeleton_provider; }

  private:
	Payload load_payload(Uri<SkinnedMesh> const& uri, Stopwatch const& stopwatch) const override;

	Ptr<SkeletonProvider> m_skeleton_provider{};
};
} // namespace levk
