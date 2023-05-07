#pragma once
#include <levk/asset/asset_list.hpp>
#include <levk/asset/asset_type.hpp>
#include <levk/asset/font_provider.hpp>
#include <levk/asset/material_provider.hpp>
#include <levk/asset/mesh_provider.hpp>
#include <levk/asset/pcm_provider.hpp>
#include <levk/asset/shader_provider.hpp>
#include <levk/asset/skeleton_provider.hpp>
#include <levk/asset/texture_provider.hpp>
#include <levk/util/signal.hpp>

namespace levk {
struct Level;

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
	SkeletalAnimationProvider const& skeletal_animation() const { return *m_providers.skeletal_animation; }
	TextureProvider& texture() const { return *m_providers.texture; }
	MaterialProvider& material() const { return *m_providers.material; }
	StaticMeshProvider& static_mesh() const { return *m_providers.static_mesh; }
	SkinnedMeshProvider& skinned_mesh() const { return *m_providers.skinned_mesh; }
	AsciiFontProvider& ascii_font() const { return *m_providers.ascii_font; }
	PcmProvider& pcm() const { return *m_providers.pcm; }

	void reload_out_of_date();
	void clear();

	asset::Type asset_type(Uri<> const& uri) const;
	MeshType mesh_type(Uri<> const& uri) const;
	AssetList build_asset_list(Uri<Level> const& uri) const;

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
	Signal<std::string_view>::Listener m_on_mount_point_changed{};
	struct {
		Ptr<ShaderProvider> shader{};
		Ptr<SkeletonProvider> skeleton{};
		Ptr<SkeletalAnimationProvider> skeletal_animation{};
		Ptr<TextureProvider> texture{};
		Ptr<MaterialProvider> material{};
		Ptr<StaticMeshProvider> static_mesh{};
		Ptr<SkinnedMeshProvider> skinned_mesh{};
		Ptr<AsciiFontProvider> ascii_font{};
		Ptr<PcmProvider> pcm{};
	} m_providers{};
};
} // namespace levk
