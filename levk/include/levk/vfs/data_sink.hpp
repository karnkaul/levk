#pragma once
#include <levk/vfs/data_source.hpp>

namespace levk {
struct DataSink : DataSource {
	virtual bool write(std::span<std::byte const> bytes, Uri<> const& uri) const = 0;
};
} // namespace levk
