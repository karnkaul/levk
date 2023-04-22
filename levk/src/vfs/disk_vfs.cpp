#include <levk/util/error.hpp>
#include <levk/vfs/disk_vfs.hpp>
#include <cassert>
#include <filesystem>
#include <fstream>

namespace levk {
namespace fs = std::filesystem;

DiskVfs::DiskVfs(std::string_view mount_point) {
	auto path = fs::path{mount_point}.generic_string();
	if (!fs::is_directory(path)) { throw Error{"Invalid mount point: " + path}; }
	m_mount_point = std::move(path);
	m_monitor = std::make_unique<UriMonitor>(m_mount_point);
}

ByteArray DiskVfs::read(Uri<> const& uri) const {
	if (!uri) { return {}; }
	auto const path = uri.absolute_path(mount_point());
	if (!fs::is_regular_file(path)) { return {}; }
	auto file = std::ifstream{path, std::ios::binary | std::ios::ate};
	if (!file) { return {}; }
	auto const size = file.tellg();
	file.seekg({}, std::ios::beg);
	auto ret = ByteArray{static_cast<std::size_t>(size)};
	file.read(reinterpret_cast<char*>(ret.data()), size);
	return ret;
}

bool DiskVfs::contains(Uri<> const& uri) const { return fs::is_regular_file(uri.absolute_path(mount_point())); }

bool DiskVfs::write(std::span<std::byte const> bytes, Uri<> const& uri) const {
	auto file = std::ofstream{uri.absolute_path(mount_point()), std::ios::binary};
	if (!file) { return false; }
	file.write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size_bytes()));
	return true;
}

bool DiskVfs::change_mount_point(std::string_view new_mount_point) {
	auto path = fs::path{new_mount_point}.generic_string();
	if (!fs::is_directory(path)) { return false; }
	m_mount_point = std::move(path);
	m_monitor->change_mount_point(m_mount_point);
	m_on_mount_point_changed.dispatch(m_mount_point);
	return true;
}
} // namespace levk
