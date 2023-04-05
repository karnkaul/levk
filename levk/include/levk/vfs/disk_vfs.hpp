#pragma once
#include <levk/vfs/data_sink.hpp>
#include <levk/vfs/data_source.hpp>
#include <levk/vfs/uri_monitor.hpp>

namespace levk {
class DiskVfs : public DataSink {
  public:
	DiskVfs(std::string_view mount_point);

	UriMonitor& uri_monitor() { return m_monitor; }
	void dispatch_modified() { m_monitor.dispatch_modified(); }

	std::string_view mount_point() const final { return m_mount_point; }
	ByteArray read(Uri<> const& uri) const final;
	bool contains(Uri<> const& uri) const final;
	bool write(std::span<std::byte const> bytes, Uri<> const& uri) const final;

  private:
	UriMonitor m_monitor;
	std::string m_mount_point{};
};
} // namespace levk
