#include <levk/font/static_font_atlas.hpp>
#include <levk/util/zip_ranges.hpp>

namespace levk {
namespace {
constexpr auto printable_codepoint_count_v{static_cast<std::size_t>(Codepoint::eAsciiEnd) - static_cast<std::size_t>(Codepoint::eAsciiStart) + 1};

constexpr std::array<Codepoint, printable_codepoint_count_v + 1> printable_codepoints() {
	auto ret = std::array<Codepoint, printable_codepoint_count_v + 1>{};
	std::size_t index{};
	ret[index++] = Codepoint::eTofu;
	for (Codepoint c{Codepoint::eAsciiStart}; c <= Codepoint::eAsciiEnd; c = static_cast<Codepoint>(static_cast<int>(c) + 1)) { ret[index++] = c; }
	return ret;
}

std::span<Codepoint const> get_codepoints(std::span<Codepoint const> in) {
	if (!in.empty()) { return in; }
	static constexpr auto ascii_v{printable_codepoints()};
	return ascii_v;
}

constexpr Extent2D ceil_pot(Extent2D const in) {
	auto ret = Extent2D{1u, 1u};
	while (ret.x < in.x) { ret.x <<= 1; }
	while (ret.y < in.y) { ret.y <<= 1; }
	return ret;
}

struct SlotMap {
	std::unordered_map<Codepoint, GlyphSlot> map{};
	Extent2D avg_extent{};
	std::uint32_t line_count{};

	static SlotMap make(std::span<Codepoint const> codepoints, GlyphSlot::Factory& slot_factory) {
		auto ret = SlotMap{};
		auto total_extent = Extent2D{};
		auto extent_count = std::uint32_t{};
		for (auto const codepoint : codepoints) {
			if (ret.map.contains(codepoint)) { continue; }
			auto slot = slot_factory.slot_for(codepoint);
			if (!slot) { continue; }
			if (slot.has_pixmap()) {
				total_extent += slot.pixmap.extent;
				++extent_count;
			}
			ret.map.insert_or_assign(codepoint, std::move(slot));
		}
		ret.line_count = static_cast<std::uint32_t>(std::sqrt(static_cast<float>(extent_count) + 0.5f));
		ret.avg_extent = extent_count > 0 ? total_extent / static_cast<std::uint32_t>(extent_count) : Extent2D{};
		return ret;
	}
};

struct Entry {
	FontGlyph glyph{};
	TextureAtlas::Cell cell{};
};
} // namespace

StaticFontAtlas::StaticFontAtlas(CreateInfo const& create_info) : m_height(clamp(create_info.height)) {
	if (!create_info.slot_factory->set_height(m_height)) { return; }

	auto const codepoints = get_codepoints(create_info.codepoints);
	auto slot_map = SlotMap::make(codepoints, *create_info.slot_factory);
	auto slot_count_sqrt = 1u;
	while (slot_count_sqrt * slot_count_sqrt < codepoints.size()) { ++slot_count_sqrt; }
	auto atlas_ci = TextureAtlas::CreateInfo{};
	atlas_ci.initial_extent = ceil_pot(slot_count_sqrt * (slot_map.avg_extent + atlas_ci.padding));

	m_atlas.emplace(create_info.texture_provider, create_info.texture_uri, atlas_ci);

	auto entries = std::unordered_map<Codepoint, Entry>{};
	auto pixmaps = std::vector<DynPixelMap>{};
	entries.reserve(codepoints.size());
	pixmaps.reserve(codepoints.size());
	auto add_entry = [&](TextureAtlas::Writer& writer, Codepoint const c) {
		auto slot_it = slot_map.map.find(c);
		if (slot_it == slot_map.map.end()) { return; }
		auto const& slot = slot_it->second;
		glm::ivec2 const advance = {slot.advance.x >> 6, slot.advance.y >> 6};
		auto entry = Entry{.glyph = FontGlyph{.advance = advance, .left_top = slot.left_top}};
		if (slot.has_pixmap()) {
			auto& pixels = pixmaps.emplace_back(slot.pixmap.extent);
			for (auto [rgba, pixel] : zip_ranges(pixels.span(), slot.pixmap.storage.span())) {
				rgba.channels.x = rgba.channels.y = rgba.channels.z = rgba.channels.w = static_cast<std::uint8_t>(pixel);
			}
			entry.cell = writer.write(pixels.view());
			entry.glyph.extent = slot.pixmap.extent;
		}
		entries.insert_or_assign(c, std::move(entry));
	};

	{
		auto writer = TextureAtlas::Writer{*m_atlas};
		for (auto const c : codepoints) { add_entry(writer, c); }
	}

	for (auto& [codepoint, entry] : entries) {
		entry.glyph.uv_rect = m_atlas->uv_rect_for(entry.cell);
		m_glyphs.insert_or_assign(codepoint, std::move(entry.glyph));
	}
}

Ptr<FontGlyph const> StaticFontAtlas::glyph_for(Codepoint const codepoint) const {
	if (auto it = m_glyphs.find(codepoint); it != m_glyphs.end()) { return &it->second; }
	return {};
}
} // namespace levk
