#include <djson/json.hpp>
#include <levk/vfs/data_sink.hpp>

namespace levk {
bool DataSink::write_text(std::string_view const text, Uri<> const& uri) const {
	return write({reinterpret_cast<std::byte const*>(text.data()), text.size()}, uri);
}

bool DataSink::write_json(dj::Json const& json, Uri<> const& uri) const { return write_text(dj::to_string(json), uri); }
} // namespace levk
