#pragma once
#include <levk/uri.hpp>
#include <levk/util/dyn_array.hpp>
#include <levk/util/ptr.hpp>
#include <levk/util/signal.hpp>

namespace dj {
class Json;
}

namespace levk {
class UriMonitor;

class DataSource {
  public:
	using OnMountPointChanged = Signal<std::string_view>;

	virtual ~DataSource() = default;

	static std::string_view as_text(std::span<std::byte const> bytes) { return {reinterpret_cast<char const*>(bytes.data()), bytes.size()}; }

	virtual std::string_view mount_point() const = 0;
	virtual ByteArray read(Uri<> const& uri) const = 0;
	virtual bool contains(Uri<> const& uri) const = 0;

	virtual Ptr<UriMonitor> uri_monitor() const { return {}; }

	std::string read_text(Uri<> const& uri) const;
	dj::Json read_json(Uri<> const& uri, std::string_view extension = ".json") const;

	Uri<> trim_to_uri(std::string_view path) const;
	OnMountPointChanged& on_mount_point_changed() const { return m_on_mount_point_changed; }

  protected:
	std::string m_mount_point{};
	mutable OnMountPointChanged m_on_mount_point_changed{};
};
} // namespace levk
