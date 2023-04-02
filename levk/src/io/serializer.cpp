#include <levk/io/serializer.hpp>

namespace levk {
Serializer::Result<Serializable> Serializer::deserialize(dj::Json const& json) const {
	auto const type_name = json["type_name"].as<std::string>();
	if (type_name.empty()) {
		logger::warn("[Serializer] Missing type_name in JSON");
		return {};
	}
	auto it = m_entries.find(type_name);
	if (it == m_entries.end()) {
		logger::warn("[Serializer] Unrecognized type_name in JSON: [{}]", type_name);
		return {};
	}
	auto const& entry = it->second;
	assert(entry.type_id != TypeId{});
	auto ret = entry.factory();
	if (!ret->deserialize(json)) {
		logger::warn("[Serializer] Failed to deserialize [{}]", type_name);
		return {};
	}
	return {std::move(ret), entry.type_id};
}

dj::Json Serializer::serialize(Serializable const& serializable) const {
	auto ret = dj::Json{};
	auto const type_name = serializable.type_name();
	ret["type_name"] = type_name;
	if (!serializable.serialize(ret)) {
		logger::warn("[Serializer] Failed to serialize [{}]", type_name);
		return {};
	}
	return ret;
}
} // namespace levk
