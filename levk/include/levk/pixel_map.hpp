#pragma once
#include <glm/vec2.hpp>
#include <levk/rgba.hpp>
#include <levk/util/dyn_array.hpp>
#include <array>

namespace levk {
using Index2D = glm::tvec2<std::size_t>;
using Extent2D = glm::uvec2;

constexpr std::size_t to_1d_index(Index2D const index2d, std::size_t columns) { return index2d.y * columns + index2d.x; }

class PixelMap {
  public:
	struct View;

	constexpr PixelMap(std::span<Rgba> rgba, Extent2D extent) : m_pixels(rgba), m_extent(extent) {}

	constexpr Extent2D extent() const { return m_extent; }
	constexpr bool is_valid(Index2D index) const { return m_extent.x > 0 && to_1d_index(index, m_extent.x) < m_pixels.size(); }

	constexpr std::span<Rgba> span() { return m_pixels; }
	constexpr std::span<Rgba const> span() const { return m_pixels; }

	View view() const;

	Rgba& operator[](Index2D index);
	Rgba const& operator[](Index2D index) const;

  protected:
	PixelMap() = default;

	constexpr void repoint(std::span<Rgba> pixels, Extent2D extent) {
		m_pixels = pixels;
		m_extent = extent;
	}

	std::span<Rgba> m_pixels{};
	Extent2D m_extent{};
};

struct PixelMap::View {
	std::span<std::byte const> bytes{};
	Extent2D extent{};
};

template <std::uint32_t Width, std::uint32_t Height>
class FixedPixelMap : public PixelMap {
  public:
	static constexpr auto extent_v = Extent2D{Width, Height};
	static constexpr auto size_v = static_cast<std::size_t>(Width * Height);

	constexpr FixedPixelMap() : PixelMap(m_storage, {Width, Height}) {}
	constexpr FixedPixelMap(std::array<Rgba, size_v> const& values) : m_storage(values) { this->repoint(m_storage, {Width, Height}); }

	constexpr FixedPixelMap(FixedPixelMap const& rhs) : m_storage(rhs.m_storage) { this->repoint(m_storage, rhs.m_extent); }

	constexpr FixedPixelMap& operator=(FixedPixelMap const& rhs) {
		m_storage = rhs.m_storage;
		this->repoint(m_storage, rhs.m_extent);
		return *this;
	}

  private:
	std::array<Rgba, size_v> m_storage{};
};

class DynPixelMap : public PixelMap {
  public:
	DynPixelMap(Extent2D extent) : m_storage(static_cast<std::size_t>(extent.x * extent.y)) { repoint(m_pixels, extent); }

  private:
	DynArray<Rgba> m_storage{};
};

ByteArray to_byte_array(PixelMap const& pixel_map);
} // namespace levk
