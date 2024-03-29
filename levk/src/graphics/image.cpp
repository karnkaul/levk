#include <stb/stb_image.h>
#include <levk/graphics/image.hpp>
#include <levk/util/logger.hpp>
#include <cstring>

namespace levk {
namespace {
auto const g_log{Logger{"Image"}};
}

void Image::Storage::Deleter::operator()(Storage const& image) const {
	if (image.data) { stbi_image_free(const_cast<std::byte*>(image.data)); }
}

Image::Image(std::span<std::byte const> compressed, std::string name) : m_name{std::move(name)} {
	int x, y, channels;
	auto ptr = stbi_load_from_memory(reinterpret_cast<stbi_uc const*>(compressed.data()), static_cast<int>(compressed.size()), &x, &y, &channels, 4);
	if (!ptr) {
		g_log.error("Failed to decompress [{}]", m_name);
		return;
	}
	m_storage = Storage{static_cast<std::size_t>(x * y * 4), reinterpret_cast<std::byte const*>(ptr)};
	m_extent = glm::uvec2{glm::ivec2{x, y}};
}

Image::Image(char const* file_path, std::string name) : m_name{std::move(name)} {
	int x, y, channels;
	auto ptr = stbi_load(file_path, &x, &y, &channels, 4);
	if (!ptr) {
		g_log.error("Failed to decompress [{}]", m_name);
		return;
	}
	m_storage = Storage{static_cast<std::size_t>(x * y * 4), reinterpret_cast<std::byte const*>(ptr)};
	m_extent = glm::uvec2{glm::ivec2{x, y}};
}

auto Image::view() const -> View { return View{.storage = m_storage.get().bytes(), .extent = m_extent}; }

Image::operator bool() const {
	if (m_extent.x == 0 || m_extent.y == 0) { return false; }
	return m_storage.get().data != nullptr;
}
} // namespace levk
