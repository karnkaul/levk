#pragma once
#include <levk/uri.hpp>
#include <levk/util/dyn_array.hpp>

namespace dj {
class Json;
}

namespace levk {
class DataSource {
  public:
	virtual ~DataSource() = default;

	static std::string_view as_text(std::span<std::byte const> bytes) { return {reinterpret_cast<char const*>(bytes.data()), bytes.size()}; }

	virtual std::string_view mount_point() const = 0;
	virtual ByteArray read(Uri<> const& uri) const = 0;

	std::string read_text(Uri<> const& uri) const;
	dj::Json read_json(Uri<> const& uri, std::string_view extension = ".json") const;

	Uri<> trim_to_uri(std::string_view path) const;

  protected:
	std::string m_mount_point{};
};
} // namespace levk
