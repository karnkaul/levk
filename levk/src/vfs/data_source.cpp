#include <djson/json.hpp>
#include <levk/util/logger.hpp>
#include <levk/vfs/data_source.hpp>
#include <filesystem>

namespace levk {
namespace fs = std::filesystem;

namespace {
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
