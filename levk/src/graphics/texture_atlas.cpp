#include <levk/asset/material_provider.hpp>
#include <levk/asset/texture_provider.hpp>
#include <levk/graphics/render_device.hpp>
#include <levk/graphics/texture_atlas.hpp>
#include <optional>

namespace levk {
namespace {
constexpr auto blank_v{Rgba{.channels = {}}};

constexpr std::uint32_t next_pot(std::uint32_t const in) {
	auto ret = std::uint32_t{1};
	while (ret < in) { ret <<= 1; }
	return ret;
}

constexpr Extent2D make_new_extent(Extent2D existing, std::optional<std::uint32_t> x, std::optional<std::uint32_t> y) {
	if (x) { existing.x = *x; }
	if (y) { existing.y = *y; }
	return existing;
}

Texture make_texture(RenderDevice& device, Extent2D extent) {
	auto image = DynPixelMap{extent};
	for (auto& rgba : image.span()) { rgba = blank_v; }
	auto sampler = TextureSampler{};
	sampler.wrap_s = sampler.wrap_t = TextureSampler::Wrap::eClampEdge;
	sampler.min = sampler.mag = TextureSampler::Filter::eLinear;
	return device.make_texture(image.view(), Texture::CreateInfo{.mip_mapped = false, .sampler = sampler});
}
} // namespace

TextureAtlas::TextureAtlas(TextureProvider& provider, Uri<Texture> uri, CreateInfo const& create_info)
	: m_uri(std::move(uri)), m_provider(&provider), m_padding(create_info.padding), m_cursor(create_info.padding) {
	provider.add(m_uri, make_texture(provider.render_device(), create_info.initial_extent));
}

UvRect TextureAtlas::uv_rect_for(Cell const& cell) const {
	auto* texture = find_texture();
	if (!texture) { return {}; }
	glm::vec2 const image_extent = texture->extent();
	glm::vec2 const top_left = cell.top_left();
	if (top_left.x > image_extent.x || top_left.y > image_extent.y) { return {}; }
	glm::vec2 const cell_extent = cell.bottom_right() - cell.top_left();
	return UvRect{.lt = top_left / image_extent, .rb = (top_left + cell_extent) / image_extent};
}

Ptr<Texture> TextureAtlas::find_texture() const { return m_provider->find(m_uri); }

TextureAtlas::Writer::~Writer() {
	auto* texture = m_out.find_texture();
	if (!texture) { return; }
	if (m_new_extent.x > 0) { texture->resize_canvas(m_new_extent, blank_v); }
	texture->write(m_writes);
}

auto TextureAtlas::Writer::write(Image::View const image) -> Cell {
	auto* texture = m_out.find_texture();
	if (!texture) { return {}; }
	auto const current_extent = m_new_extent.x == 0 ? texture->extent() : m_new_extent;
	auto const required_extent = image.extent + m_out.m_padding;
	if (m_out.m_cursor.x + required_extent.x > texture->extent().x) {
		m_out.m_cursor.y += m_out.m_max_height + m_out.m_padding.y; // line break
		m_out.m_cursor.x = m_out.m_padding.x;						// carriage return
		m_out.m_max_height = {};
	}
	auto new_width = std::optional<std::uint32_t>{};
	auto new_height = std::optional<std::uint32_t>{};
	if (m_out.m_cursor.x + required_extent.x > current_extent.x) { new_width = next_pot(image.extent.x); }
	if (m_out.m_cursor.y + required_extent.y > current_extent.y) { new_height = next_pot(current_extent.y + required_extent.y); }
	if (new_width || new_height) { m_new_extent = make_new_extent(current_extent, new_width, new_height); }
	auto const ret = Cell{.lt = m_out.m_cursor, .rb = m_out.m_cursor + image.extent};
	m_out.m_cursor.x += required_extent.x;
	m_out.m_max_height = std::max(m_out.m_max_height, required_extent.y);
	m_writes.push_back(ImageWrite{image, ret.top_left()});
	return ret;
}
} // namespace levk
