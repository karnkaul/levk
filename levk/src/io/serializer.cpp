#include <levk/io/serializer.hpp>

namespace levk {
namespace {
auto const g_log{Logger{"Serializer"}};
}

Serializer::Result<Serializable> Serializer::deserialize(dj::Json const& json) const {
	auto const type_name = json["type_name"].as<std::string>();
	if (type_name.empty()) {
		g_log.warn("Missing type_name in JSON");
		return {};
	}
	auto it = m_entries.find(type_name);
	if (it == m_entries.end()) {
		g_log.warn("Unrecognized type_name in JSON: [{}]", type_name);
		return {};
	}
	auto const& entry = it->second;
	assert(entry.type_id != TypeId{});
	auto ret = Result<Serializable>{.type_name = it->first, .type_id = entry.type_id};
	auto value = entry.factory();
	if (!value || !value->deserialize(json)) {
		g_log.warn("Failed to deserialize [{}]", type_name);
		return ret;
	}
	ret.value = std::move(value);
	return ret;
}

dj::Json Serializer::serialize(Serializable const& serializable) const {
	auto ret = dj::Json{};
	auto const type_name = serializable.type_name();
	ret["type_name"] = type_name;
	if (!serializable.serialize(ret)) {
		g_log.warn("Failed to serialize [{}]", type_name);
		return {};
	}
	return ret;
}
} // namespace levk
