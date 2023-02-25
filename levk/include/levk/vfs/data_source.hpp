#pragma once
#include <levk/uri.hpp>
#include <levk/util/dyn_array.hpp>

namespace levk {
struct DataSource {
	virtual ~DataSource() = default;

	static std::string_view as_text(std::span<std::byte const> bytes) { return {reinterpret_cast<char const*>(bytes.data()), bytes.size()}; }

	virtual ByteArray read(Uri<> const& uri) const = 0;
};
} // namespace levk
