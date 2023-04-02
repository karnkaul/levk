#include <levk/vfs/data_source.hpp>
#include <levk/vfs/uri_monitor.hpp>
#include <filesystem>

namespace levk {
namespace fs = std::filesystem;

UriMonitor::UriMonitor(std::string mount_point) : m_mount_point(std::move(mount_point)) {}

UriMonitor::Timestamp UriMonitor::last_modified(Uri<> const& uri) const {
	if (!uri || m_mount_point.empty()) { return {}; }
	auto const ret = fs::last_write_time(uri.absolute_path(mount_point()));
	return std::chrono::duration_cast<Timestamp>(ret.time_since_epoch());
}

UriMonitor::OnModified& UriMonitor::on_modified(Uri<> const& uri) {
	auto lock = std::scoped_lock{m_mutex};
	auto [it, _] = m_monitors.emplace(uri, Monitor{.timestamp = last_modified(uri)});
	return it->second.on_modified;
}

void UriMonitor::dispatch_modified() {
	auto lock = std::unique_lock{m_mutex};
	auto modified = std::vector<std::pair<Uri<> const*, Monitor*>>{};
	for (auto& [uri, monitor] : m_monitors) {
		Timestamp const lwt = last_modified(uri);
		if (monitor.timestamp == Timestamp{} || lwt > monitor.timestamp) {
			monitor.timestamp = lwt;
			modified.push_back({&uri, &monitor});
		}
	}
	lock.unlock();
	for (auto [uri, monitor] : modified) { monitor->on_modified(*uri); }
}
} // namespace levk
