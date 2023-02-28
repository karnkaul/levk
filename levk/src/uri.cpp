#include <fmt/format.h>
#include <levk/uri.hpp>
#include <levk/util/hash_combine.hpp>
#include <filesystem>

namespace levk {
namespace fs = std::filesystem;

Uri<void>::Uri(std::string value) : m_value(std::move(value)) {
	for (char const c : m_value) { hash_combine(m_hash, c); }
}

std::string Uri<void>::absolute_path(std::string_view root_dir) const { return (fs::path{root_dir} / m_value).generic_string(); }

std::string Uri<void>::parent() const {
	auto it = m_value.find_last_of('/');
	if (it == std::string::npos) { return {}; }
	return m_value.substr(0, it);
}

std::string Uri<void>::append(std::string_view suffix) const { return (fs::path{m_value} / suffix).generic_string(); }

std::string Uri<void>::concat(std::string_view suffix) const { return fmt::format("{}{}", m_value, suffix); }

std::string Uri<void>::Builder::build(std::string_view uri) const { return (fs::path{root_dir} / sub_dir / uri).generic_string(); }
} // namespace levk
