#include <levk/asset/font_provider.hpp>
#include <levk/asset/texture_provider.hpp>
#include <levk/font/font_library.hpp>
#include <levk/util/logger.hpp>
#include <filesystem>

namespace levk {
namespace fs = std::filesystem;

AsciiFontProvider::AsciiFontProvider(TextureProvider& texture_provider, FontLibrary const& font_library)
	: GraphicsAssetProvider<AsciiFont>(texture_provider.render_device(), texture_provider.data_source(), texture_provider.uri_monitor()),
	  m_texture_provider(&texture_provider), m_font_library(&font_library) {}

auto AsciiFontProvider::load_payload(Uri<AsciiFont> const& uri) const -> Payload {
	auto ret = Payload{};
	if (fs::path{uri.value()}.extension() == ".json") {
		logger::warn("[FontProvider] JSON deserialization not implemented (URI: [{}])", uri.value());
		return {};
	}
	auto bytes = data_source().read(uri);
	if (!bytes) {
		logger::error("[FontProvider] Failed to read font [{}]", uri.value());
		return {};
	}
	auto slot_factory = m_font_library->load(std::move(bytes));
	if (!slot_factory) {
		logger::warn("[FontProvider] Failed to load font [{}]", uri.value());
		return {};
	}
	auto uri_prefix = Uri<Texture>{(fs::path{uri.value()} / "atlas").generic_string()};
	ret.asset = AsciiFont{std::move(slot_factory), *m_texture_provider, std::move(uri_prefix)};
	if (!*ret.asset) {
		logger::warn("[FontProvider] Failed to load font [{}]", uri.value());
		return {};
	}
	logger::info("[FontProvider] AsciiFont Loaded [{}]", uri.value());
	ret.dependencies.push_back(uri);
	return ret;
}
} // namespace levk
