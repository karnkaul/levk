#pragma once
#include <levk/asset/asset_provider.hpp>
#include <levk/graphics/texture.hpp>

namespace levk {
class ThreadPool;

class TextureProvider : public GraphicsAssetProvider<Texture> {
  public:
	static constexpr ColourSpace to_colour_space(std::string_view const str) {
		if (str == "linear") { return ColourSpace::eLinear; }
		return ColourSpace::eSrgb;
	}

	TextureProvider(NotNull<RenderDevice*> render_device, NotNull<DataSource const*> data_source);

	Texture const& get(Uri<Texture> const& uri, Uri<Texture> const& fallback = "white") const;
	void clear() override;

	Ptr<Texture const> white() const { return find("white"); }
	Ptr<Texture const> black() const { return find("black"); }

  private:
	void add_default_textures();

	Payload load_payload(Uri<Texture> const& uri, Stopwatch const& stopwatch) const override;
};

class CubemapProvider : public GraphicsAssetProvider<Cubemap> {
  public:
	CubemapProvider(NotNull<RenderDevice*> render_device, NotNull<DataSource const*> data_source, NotNull<ThreadPool*> thread_pool);

	Cubemap const& get(Uri<Cubemap> const& uri, Uri<Cubemap> const& fallback = "white") const;
	void clear() override;

	Ptr<Cubemap const> white() const { return find("white"); }
	Ptr<Cubemap const> black() const { return find("black"); }

	ThreadPool& thread_pool() const { return *m_thread_pool; }

  private:
	void add_default_cubemaps();

	Payload load_payload(Uri<Cubemap> const& uri, Stopwatch const& stopwatch) const override;

	NotNull<ThreadPool*> m_thread_pool;
};
} // namespace levk
