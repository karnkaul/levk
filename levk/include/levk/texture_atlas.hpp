#pragma once
#include <levk/rect.hpp>
#include <levk/texture.hpp>
#include <levk/uri.hpp>
#include <levk/util/ptr.hpp>

namespace levk {
class TextureProvider;

struct TextureAtlasCreateInfo {
	Extent2D initial_extent{512u, 512u};
	glm::uvec2 padding{2u, 2u};
};

class TextureAtlas {
  public:
	using Cell = Rect2D<std::uint32_t>;
	using CreateInfo = TextureAtlasCreateInfo;

	TextureAtlas(TextureProvider& texture_provider, Uri<Texture> texture_uri, CreateInfo const& create_info = {});

	TextureProvider const& texture_provider() const { return *m_provider; }

	Uri<Texture> const& texture_uri() const { return m_uri; }
	UvRect uv_rect_for(Cell const& cell) const;

	Cell inscribe(PixelMap::View pixels);

  private:
	Ptr<Texture> find_texture() const;

	Uri<Texture> m_uri{};
	Ptr<TextureProvider> m_provider{};
	glm::uvec2 m_padding{};
	glm::uvec2 m_cursor{};
	std::uint32_t m_max_height{};
};
} // namespace levk
