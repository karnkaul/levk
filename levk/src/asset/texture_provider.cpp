#include <levk/asset/texture_provider.hpp>
#include <levk/graphics/render_device.hpp>
#include <levk/util/enumerate.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/thread_pool.hpp>
#include <filesystem>

namespace levk {
namespace fs = std::filesystem;

TextureProvider::TextureProvider(NotNull<RenderDevice*> render_device, NotNull<DataSource const*> data_source)
	: GraphicsAssetProvider<Texture>(render_device, data_source, "TextureProvider") {
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
			m_logger.error("Failed to load JSON [{}]", uri.value());
			return {};
		}
		image_uri = json["image"].as_string();
		colour_space = to_colour_space(json["colour_space"].as_string());
	} else {
		image_uri = uri.value();
	}
	auto const bytes = read_bytes(image_uri);
	if (!bytes) {
		m_logger.error("Failed to load Image [{}]", image_uri);
		return {};
	}
	auto image = Image{bytes.span(), std::string{image_uri}};
	if (!image) {
		m_logger.error("Failed to create Image [{}]", image_uri);
		return {};
	}
	ret.asset.emplace(render_device().vulkan_device(), image, TextureCreateInfo{.colour_space = colour_space});
	ret.dependencies = {uri, image_uri};
	m_logger.info("[{:.3f}s] Texture loaded [{}]", stopwatch().count(), uri.value());
	return ret;
}

void TextureProvider::add_default_textures() {
	static constexpr auto white_image_v = FixedPixelMap<1, 1>{{white_v}};
	add("white", Texture{render_device().vulkan_device(), white_image_v.view(), TextureCreateInfo{.name = "white", .mip_mapped = false}});
	static constexpr auto black_image_v = FixedPixelMap<1, 1>{{black_v}};
	add("black", Texture{render_device().vulkan_device(), black_image_v.view(), TextureCreateInfo{.name = "black", .mip_mapped = false}});
}

CubemapProvider::CubemapProvider(NotNull<RenderDevice*> render_device, NotNull<DataSource const*> data_source, NotNull<ThreadPool*> thread_pool)
	: GraphicsAssetProvider<Cubemap>(render_device, data_source, "CubemapProvider"), m_thread_pool(thread_pool) {
	add_default_cubemaps();
}

Cubemap const& CubemapProvider::get(Uri<Cubemap> const& uri, Uri<Cubemap> const& fallback) const {
	if (auto* ret = find(uri)) { return *ret; }
	if (auto* ret = find(fallback)) { return *ret; }
	auto* ret = white();
	assert(ret);
	return *ret;
}

void CubemapProvider::clear() {
	GraphicsAssetProvider<Cubemap>::clear();
	add_default_cubemaps();
}

CubemapProvider::Payload CubemapProvider::load_payload(Uri<Cubemap> const& uri, Stopwatch const& stopwatch) const {
	auto ret = Payload{};
	auto path = fs::path{uri.value()};
	auto json = [&] {
		if (path.extension() == ".json") {
			return data_source().read_json(uri);
		} else if (!path.has_extension()) {
			return data_source().read_json((path / "cubemap.json").generic_string());
		}
		return dj::Json{};
	}();
	if (!json) {
		m_logger.error("Failed to load JSON for Cubemap [{}]", uri.value());
		return {};
	}
	auto const& in_images = json["images"];
	if (!in_images.is_array() || in_images.array_view().size() != 6) {
		m_logger.error("Invalid Cubemap JSON [{}]", uri.value());
		return {};
	}
	auto const colour_space = TextureProvider::to_colour_space(json["colour_space"].as_string());

	auto image_uris = std::array<std::string, 6>{};
	for (auto const [in_image, index] : enumerate(in_images.array_view())) { image_uris[index] = in_image.as<std::string>(); }

	auto bytes = std::array<ScopedFuture<ByteArray>, 6>{};
	for (auto const [image_uri, index] : enumerate(image_uris)) {
		bytes[index] = m_thread_pool->submit([this, image_uri] { return read_bytes(image_uri); });
	}

	auto images = std::array<ScopedFuture<Image>, 6>{};
	bool first{true};
	for (auto [future, index] : enumerate(bytes)) {
		if (!future.future.get()) {
			m_logger.error("Failed to load Cubemap Image {} [{}]", index, image_uris[index]);
			return {};
		}
		images[index] = m_thread_pool->submit([b = future.future.get().span(), n = image_uris[index]] { return Image{b, std::move(n)}; });
	}

	auto image_views = std::array<Image::View, 6>{};
	auto extent = Extent2D{};
	for (auto const [future, index] : enumerate(images)) {
		if (!future.future.get()) {
			m_logger.error("Failed to create Cubemap Image {} [{}]", index, image_uris[index]);
			return {};
		}
		image_views[index] = future.future.get().view();
		if (first) {
			extent = image_views[index].extent;
		} else if (image_views[index].extent != extent) {
			auto const& e = image_views[index].extent;
			m_logger.error("Mismatched Cubemap Image {} extent: [{}x{}] vs [{}x{}]", index, extent.x, extent.y, e.x, e.y);
			return {};
		}
	}

	ret.asset.emplace(render_device().vulkan_device(), image_views, TextureCreateInfo{.colour_space = colour_space});
	ret.dependencies.reserve(image_uris.size() + 1);
	ret.dependencies.push_back(uri);
	std::move(image_uris.begin(), image_uris.end(), std::back_inserter(ret.dependencies));
	m_logger.info("[{:.3f}s] Cubemap loaded [{}]", stopwatch().count(), uri.value());
	return ret;
}

void CubemapProvider::add_default_cubemaps() {
	auto const make_cubemap = [](auto const& image) {
		auto ret = std::array<Image::View, 6>{};
		for (auto& view : ret) { view = image.view(); }
		return ret;
	};
	static constexpr auto white_image_v = FixedPixelMap<1, 1>{{white_v}};
	auto const white_cubemap = make_cubemap(white_image_v);
	add("white", Cubemap{render_device().vulkan_device(), white_cubemap, TextureCreateInfo{.name = "white", .mip_mapped = false}});
	static constexpr auto black_image_v = FixedPixelMap<1, 1>{{black_v}};
	auto const black_cubemap = make_cubemap(black_image_v);
	add("black", Cubemap{render_device().vulkan_device(), black_cubemap, TextureCreateInfo{.name = "black", .mip_mapped = false}});
}
} // namespace levk
