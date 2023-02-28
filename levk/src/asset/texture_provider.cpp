#include <levk/asset/texture_provider.hpp>
#include <levk/graphics_device.hpp>
#include <levk/util/logger.hpp>

namespace levk {
TextureProvider::TextureProvider(GraphicsDevice& graphics_device, DataSource const& data_source, UriMonitor& uri_monitor)
	: GraphicsAssetProvider<Texture>(graphics_device, data_source, uri_monitor) {
	static constexpr auto white_image_v = FixedPixelMap<1, 1>{{white_v}};
	add("white", graphics_device.make_texture(white_image_v.view(), TextureCreateInfo{.name = "white", .mip_mapped = false}));
	static constexpr auto black_image_v = FixedPixelMap<1, 1>{{black_v}};
	add("black", graphics_device.make_texture(black_image_v.view(), TextureCreateInfo{.name = "black", .mip_mapped = false}));
}

Texture const& TextureProvider::get(Uri<Texture> const& uri, Uri<Texture> const& fallback) const {
	if (auto* ret = find(uri)) { return *ret; }
	if (auto* ret = find(fallback)) { return *ret; }
	auto* ret = white();
	assert(ret);
	return *ret;
}

TextureProvider::Payload TextureProvider::load_payload(Uri<Texture> const& uri) const {
	auto ret = Payload{};
	auto json = data_source().read_json(uri);
	if (!json) {
		logger::error("[TextureProvider] Failed to load JSON [{}]", uri.value());
		return {};
	}
	auto const image_uri = json["image"].as_string();
	auto const bytes = read_bytes(image_uri);
	if (!bytes) {
		logger::error("[TextureProvider] Failed to load Image [{}]", image_uri);
		return {};
	}
	auto image = Image{bytes.span(), std::string{image_uri}};
	if (!image) {
		logger::error("[TextureProvider] Failed to create Image [{}]", image_uri);
		return {};
	}
	auto const colour_space = to_colour_space(json["colour_space"].as_string());
	ret.asset = graphics_device().make_texture(image, TextureCreateInfo{.colour_space = colour_space});
	ret.dependencies = {uri, image_uri};
	logger::info("[TextureProvider] Texture loaded [{}]", uri.value());
	return ret;
}
} // namespace levk
