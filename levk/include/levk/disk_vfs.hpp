#pragma once
#include <levk/vfs/data_monitor.hpp>
#include <levk/vfs/data_sink.hpp>
#include <levk/vfs/data_source.hpp>

namespace levk {
class DiskVfs : public DataMonitor {
  public:
	explicit DiskVfs(std::string mount_point);

	DiskVfs(DiskVfs&&) noexcept;
	DiskVfs& operator=(DiskVfs&&) noexcept;
	~DiskVfs() noexcept;

	std::string_view mount_point() const;
	std::string absolute_path_for(Uri<> const& uri) const;
	Uri<> trim_to_uri(std::string_view path) const;

	ByteArray read(Uri<> const& uri) const final;
	bool write(std::span<std::byte const> bytes, Uri<> const& uri) const final;
	OnModified::Handle connect(Uri<> uri, OnModified::Callback on_modified) final;
	std::uint32_t reload_modified(int max_reloads = -1) final;

  private:
	struct Impl;
	std::unique_ptr<Impl> m_impl{};
};
} // namespace levk
