#include <levk/pixel_map.hpp>
#include <cassert>
#include <cstring>
#include <utility>

namespace levk {
Rgba& PixelMap::operator[](Index2D index) { return const_cast<Rgba&>(std::as_const(*this)[index]); }

Rgba const& PixelMap::operator[](Index2D index) const {
	auto const idx = to_1d_index(index, m_extent.x);
	assert(m_extent.x > 0 && idx < m_pixels.size());
	return m_pixels[idx];
}

PixelMap::View PixelMap::view() const {
	return View{.storage = {reinterpret_cast<std::byte const*>(m_pixels.data()), m_pixels.size_bytes()}, .extent = m_extent};
}
} // namespace levk

levk::ByteArray levk::to_byte_array(PixelMap const& pixel_map) {
	auto const span = pixel_map.span();
	auto ret = ByteArray{span.size_bytes()};
	std::memcpy(ret.data(), span.data(), ret.size());
	return ret;
}
