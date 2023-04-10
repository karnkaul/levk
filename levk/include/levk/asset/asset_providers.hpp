#pragma once
#include <levk/asset/asset_list.hpp>
#include <levk/asset/font_provider.hpp>
#include <levk/asset/material_provider.hpp>
#include <levk/asset/mesh_provider.hpp>
#include <levk/asset/shader_provider.hpp>
#include <levk/asset/skeleton_provider.hpp>
#include <levk/asset/texture_provider.hpp>

namespace levk {
class Scene;

class AssetProviders {
  public:
	struct CreateInfo {
		NotNull<RenderDevice*> render_device;
		NotNull<FontLibrary const*> font_library;
		NotNull<DataSource const*> data_source;
		NotNull<Serializer const*> serializer;
	};

	AssetProviders(CreateInfo const& create_info);

	template <typename ProviderT>
	ProviderT& add(ProviderT provider) {
		auto t = std::make_unique<Model<ProviderT>>(std::move(provider));
		auto* ret = t.get();
		m_storage.push_back(std::move(t));
		return static_cast<ProviderT&>(ret->provider);
	}

	DataSource const& data_source() const { return m_providers.shader->data_source(); }
	RenderDevice& render_device() const { return m_providers.texture->render_device(); }
	Serializer const& serializer() const { return m_providers.material->serializer(); }
	Ptr<UriMonitor> uri_monitor() const { return data_source().uri_monitor(); }

	ShaderProvider& shader() const { return *m_providers.shader; }
	SkeletonProvider& skeleton() const { return *m_providers.skeleton; }
	TextureProvider& texture() const { return *m_providers.texture; }
	MaterialProvider& material() const { return *m_providers.material; }
	StaticMeshProvider& static_mesh() const { return *m_providers.static_mesh; }
	SkinnedMeshProvider& skinned_mesh() const { return *m_providers.skinned_mesh; }
	AsciiFontProvider& ascii_font() const { return *m_providers.ascii_font; }

	void reload_out_of_date();
	void clear();

	std::string asset_type(Uri<> const& uri) const;
	MeshType mesh_type(Uri<> const& uri) const;
	AssetList build_asset_list(Uri<Scene> const& uri) const;

  private:
	struct Base {
		virtual ~Base() = default;
		virtual void reload_out_of_date() = 0;
		virtual void clear() = 0;
	};
	template <typename T>
	struct Model : Base {
		T provider;
		Model(T&& provider) : provider(std::move(provider)) {}
		void reload_out_of_date() final { provider.reload_out_of_date(); }
		void clear() final { provider.clear(); }
	};

	std::vector<std::unique_ptr<Base>> m_storage{};
	struct {
		Ptr<ShaderProvider> shader{};
		Ptr<SkeletonProvider> skeleton{};
		Ptr<TextureProvider> texture{};
		Ptr<MaterialProvider> material{};
		Ptr<StaticMeshProvider> static_mesh{};
		Ptr<SkinnedMeshProvider> skinned_mesh{};
		Ptr<AsciiFontProvider> ascii_font{};
	} m_providers{};
};
} // namespace levk
