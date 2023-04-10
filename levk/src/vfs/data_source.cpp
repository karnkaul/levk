#include <djson/json.hpp>
#include <levk/util/logger.hpp>
#include <levk/vfs/data_source.hpp>
#include <filesystem>

namespace levk {
namespace fs = std::filesystem;

namespace {
fs::path find_dir(fs::path exe, std::span<std::string_view const> patterns) {
	auto check = [patterns](fs::path const& prefix) {
		for (auto const pattern : patterns) {
			auto path = prefix / pattern;
			if (fs::is_directory(path)) { return path; }
		}
		return fs::path{};
	};
	while (!exe.empty()) {
		if (auto ret = check(exe); !ret.empty()) { return ret; }
		auto parent = exe.parent_path();
		if (exe == parent) { break; }
		exe = std::move(parent);
	}
	return {};
}

auto const g_log{Logger{"DataSource"}};
} // namespace

std::string DataSource::read_text(Uri<> const& uri) const {
	auto bytes = read(uri);
	if (!bytes) { return {}; }
	return std::string{as_text(bytes.span())};
}

dj::Json DataSource::read_json(Uri<> const& uri, std::string_view const extension) const {
	if (auto const ext = fs::path{uri.value()}.extension(); ext != extension) {
		g_log.error("Invalid JSON URI [{}]", uri.value());
		return {};
	}
	auto const bytes = read(uri);
	if (!bytes) { return {}; }
	return dj::Json::parse(as_text(bytes.span()));
}

Uri<> DataSource::trim_to_uri(std::string_view path) const {
	auto p = fs::path{path}.generic_string();
	auto const it = p.find(mount_point());
	if (it == std::string_view::npos) { return {}; }
	auto ret = Uri{p.substr(it + mount_point().size() + 1)};
	assert(ret.absolute_path(mount_point()) == p);
	return ret;
}
} // namespace levk

std::string levk::find_directory(char const* exe_path, std::span<std::string_view const> uri_patterns) {
	if (uri_patterns.empty()) { return {}; }
	return find_dir(fs::absolute(exe_path), uri_patterns).generic_string();
}
