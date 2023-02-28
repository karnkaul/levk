#pragma once
#include <levk/asset/material_provider.hpp>
#include <levk/skeleton.hpp>
#include <levk/skinned_mesh.hpp>
#include <levk/static_mesh.hpp>

namespace levk {
template <typename Type>
class MeshProviderCommon : public GraphicsAssetProvider<Type> {
  public:
	MeshProviderCommon(MaterialProvider& material_provider)
		: GraphicsAssetProvider<Type>(material_provider.graphics_device(), material_provider.data_source(), material_provider.uri_monitor()),
		  m_material_provider(&material_provider) {}

	MaterialProvider& material_provider() const { return *m_material_provider; }

  private:
	Ptr<MaterialProvider> m_material_provider{};
};

class SkeletonProvider : public AssetProvider<Skeleton> {
  public:
	using AssetProvider<Skeleton>::AssetProvider;

  private:
	Payload load_payload(Uri<Skeleton> const& uri) const override;
};

class StaticMeshProvider : public MeshProviderCommon<StaticMesh> {
  public:
	using MeshProviderCommon<StaticMesh>::MeshProviderCommon;

  private:
	Payload load_payload(Uri<StaticMesh> const& uri) const override;
};

class SkinnedMeshProvider : public MeshProviderCommon<SkinnedMesh> {
  public:
	SkinnedMeshProvider(SkeletonProvider& skeleton_provider, MaterialProvider& material_provider);

	SkeletonProvider& skeleton_provider() const { return *m_skeleton_provider; }

  private:
	Payload load_payload(Uri<SkinnedMesh> const& uri) const override;

	Ptr<SkeletonProvider> m_skeleton_provider{};
};
} // namespace levk
