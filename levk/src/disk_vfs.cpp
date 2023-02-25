#include <levk/disk_vfs.hpp>
#include <levk/util/error.hpp>
#include <cassert>
#include <filesystem>
#include <fstream>

namespace levk {
namespace fs = std::filesystem;

struct DiskVfs::Impl {
	struct Entry {
		OnModified on_modified{};
		fs::file_time_type last_write_time{};
	};

	std::unordered_map<Uri<>, Entry, Uri<>::Hasher> map{};
	std::string mount_point{};
};

DiskVfs::DiskVfs(std::string mount_point) : m_impl(std::make_unique<Impl>()) {
	if (!fs::is_directory(mount_point)) { throw Error{"Invalid mount point: " + mount_point}; }
	m_impl->mount_point = std::move(mount_point);
}

DiskVfs::DiskVfs(DiskVfs&&) noexcept = default;
DiskVfs& DiskVfs::operator=(DiskVfs&&) noexcept = default;
DiskVfs::~DiskVfs() noexcept = default;

std::string_view DiskVfs::mount_point() const { return m_impl->mount_point; }

std::string DiskVfs::absolute_path_for(Uri<> const& uri) const {
	if (!uri) { return {}; }
	return uri.absolute_path(m_impl->mount_point);
}

Uri<> DiskVfs::trim_to_uri(std::string_view path) const {
	auto const it = path.find(m_impl->mount_point);
	if (it == std::string_view::npos) { return {}; }
	auto ret = Uri{path.substr(it + 1)};
	assert(ret.absolute_path(m_impl->mount_point) == path);
	return ret;
}

ByteArray DiskVfs::read(Uri<> const& uri) const {
	if (!uri) { return {}; }
	auto const path = absolute_path_for(uri);
	if (!fs::is_regular_file(path)) { return {}; }
	auto file = std::ifstream{path, std::ios::binary | std::ios::ate};
	if (!file) { return {}; }
	auto const size = file.tellg();
	file.seekg({}, std::ios::beg);
	auto ret = ByteArray{static_cast<std::size_t>(size)};
	file.read(reinterpret_cast<char*>(ret.data()), size);
	return ret;
}

bool DiskVfs::write(std::span<std::byte const> bytes, Uri<> const& uri) const {
	auto file = std::ofstream{absolute_path_for(uri), std::ios::binary};
	if (!file) { return false; }
	file.write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size_bytes()));
	return true;
}

DiskVfs::OnModified::Handle DiskVfs::connect(Uri<> uri, OnModified::Callback on_modified) {
	auto& entry = m_impl->map[uri];
	if (auto bytes = read(uri)) {
		entry.last_write_time = fs::last_write_time(uri.absolute_path(m_impl->mount_point));
		on_modified(uri);
	}
	return entry.on_modified.connect(std::move(on_modified));
}

std::uint32_t DiskVfs::reload_modified(int max_reloads) {
	if (max_reloads == 0) { return 0; }
	auto reloaded = std::uint32_t{};
	for (auto& [uri, entry] : m_impl->map) {
		auto const path = uri.absolute_path(m_impl->mount_point);
		if (!fs::is_regular_file(path)) { continue; }
		auto const lwt = fs::last_write_time(path);
		if (entry.last_write_time == fs::file_time_type{} || lwt > entry.last_write_time) {
			entry.on_modified(uri);
			entry.last_write_time = lwt;
			++reloaded;
			if (max_reloads >= 0) {
				if (max_reloads == 0) { break; }
				--max_reloads;
			}
		}
	}
	return reloaded;
}
} // namespace levk
