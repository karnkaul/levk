#include <levk/asset/material_provider.hpp>
#include <levk/asset/texture_provider.hpp>

namespace levk {
MaterialProvider::MaterialProvider(NotNull<TextureProvider*> texture_provider, NotNull<Serializer const*> serializer)
	: GraphicsAssetProvider<UMaterial>(&texture_provider->render_device(), &texture_provider->data_source(), &texture_provider->uri_monitor()),
	  m_texture_provider(texture_provider), m_serializer(serializer) {}

MaterialProvider::Payload MaterialProvider::load_payload(Uri<UMaterial> const& uri) const {
	auto ret = Payload{};
	auto json = data_source().read_json(uri);
	if (!json) {
		logger::error("[MaterialProvider] Failed to load JSON [{}]", uri.value());
		return {};
	}
	auto mat = m_serializer->deserialize_as<Material>(json);
	if (!mat) {
		logger::error("[MaterialProvider] Failed to deserialize Material [{}]", uri.value());
		return {};
	}
	for (auto const& tex_uri : mat.value->textures.uris) {
		if (tex_uri) { m_texture_provider->load(tex_uri); }
	}
	ret.asset.emplace(std::move(mat.value));
	ret.dependencies.push_back(uri);
	logger::info("[MaterialProvider] Material loaded [{}]", uri.value());
	return ret;
}
} // namespace levk
