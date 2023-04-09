#pragma once
#include <levk/asset/asset_provider.hpp>
#include <levk/graphics/texture.hpp>

namespace levk {
class TextureProvider : public GraphicsAssetProvider<Texture> {
  public:
	static constexpr ColourSpace to_colour_space(std::string_view const str) {
		if (str == "linear") { return ColourSpace::eLinear; }
		return ColourSpace::eSrgb;
	}

	TextureProvider(NotNull<RenderDevice*> render_device, NotNull<DataSource const*> data_source, Ptr<UriMonitor> uri_monitor = {});

	Texture const& get(Uri<Texture> const& uri, Uri<Texture> const& fallback = "white") const;

	Ptr<Texture const> white() const { return find("white"); }
	Ptr<Texture const> black() const { return find("black"); }

  private:
	Payload load_payload(Uri<Texture> const& uri) const override;
};
} // namespace levk
