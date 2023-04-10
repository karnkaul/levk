#include <levk/asset/texture_provider.hpp>
#include <levk/graphics/render_device.hpp>
#include <levk/util/logger.hpp>
#include <filesystem>

namespace levk {
namespace fs = std::filesystem;

TextureProvider::TextureProvider(NotNull<RenderDevice*> render_device, NotNull<DataSource const*> data_source)
	: GraphicsAssetProvider<Texture>(render_device, data_source) {
	add_default_textures();
}

Texture const& TextureProvider::get(Uri<Texture> const& uri, Uri<Texture> const& fallback) const {
	if (auto* ret = find(uri)) { return *ret; }
	if (auto* ret = find(fallback)) { return *ret; }
	auto* ret = white();
	assert(ret);
	return *ret;
}

void TextureProvider::clear() {
	GraphicsAssetProvider<Texture>::clear();
	add_default_textures();
}

TextureProvider::Payload TextureProvider::load_payload(Uri<Texture> const& uri, Stopwatch const& stopwatch) const {
	auto ret = Payload{};
	auto image_uri = std::string{};
	auto colour_space = ColourSpace::eSrgb;
	if (fs::path{uri.value()}.extension() == ".json") {
		auto json = data_source().read_json(uri);
		if (!json) {
			logger::error("[TextureProvider] Failed to load JSON [{}]", uri.value());
			return {};
		}
		image_uri = json["image"].as_string();
		colour_space = to_colour_space(json["colour_space"].as_string());
	} else {
		image_uri = uri.value();
	}
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
	ret.asset.emplace(render_device().vulkan_device(), image, TextureCreateInfo{.colour_space = colour_space});
	ret.dependencies = {uri, image_uri};
	logger::info("[{:.3f}s] [TextureProvider] Texture loaded [{}]", stopwatch().count(), uri.value());
	return ret;
}

void TextureProvider::add_default_textures() {
	static constexpr auto white_image_v = FixedPixelMap<1, 1>{{white_v}};
	add("white", Texture{render_device().vulkan_device(), white_image_v.view(), TextureCreateInfo{.name = "white", .mip_mapped = false}});
	static constexpr auto black_image_v = FixedPixelMap<1, 1>{{black_v}};
	add("black", Texture{render_device().vulkan_device(), black_image_v.view(), TextureCreateInfo{.name = "black", .mip_mapped = false}});
}
} // namespace levk
