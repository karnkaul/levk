#include <fmt/format.h>
#include <levk/uri.hpp>
#include <levk/util/hash_combine.hpp>

namespace levk {
namespace {
std::string append_path(std::string_view prefix, std::string_view suffix) {
	bool const slash = !prefix.empty() && !suffix.empty();
	return fmt::format("{}{}{}", prefix, slash ? "/" : "", suffix);
}

template <std::size_t N>
std::string append_paths(std::string_view prefix, std::string_view const (&suffixes)[N]) {
	auto ret = std::string{prefix};
	for (auto suffix : suffixes) { ret = append_path(ret, suffix); }
	return ret;
}
} // namespace

Uri::Uri(std::string value) : m_value(std::move(value)) {
	for (char const c : m_value) { hash_combine(m_hash, c); }
}

std::string Uri::absolute_path(std::string_view root_dir) const { return append_path(root_dir, m_value); }

std::string Uri::parent() const {
	auto it = m_value.find_last_of('/');
	if (it == std::string::npos) { return {}; }
	return m_value.substr(0, it);
}

std::string Uri::append(std::string_view suffix) const { return append_path(m_value, suffix); }

std::string Uri::concat(std::string_view suffix) const { return fmt::format("{}{}", m_value, suffix); }

std::string Uri::Builder::build(std::string_view uri) const { return append_paths(root_dir, {sub_dir, uri}); }
} // namespace levk
