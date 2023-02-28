#pragma once
#include <levk/vfs/data_source.hpp>

namespace levk {
struct DataSink : DataSource {
	virtual bool write(std::span<std::byte const> bytes, Uri<> const& uri) const = 0;

	bool write_text(std::string_view const text, Uri<> const& uri) const;
	bool write_json(dj::Json const& json, Uri<> const& uri) const;
};
} // namespace levk
