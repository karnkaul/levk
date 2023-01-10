#include <levk/serializer.hpp>
#include <levk/util/logger.hpp>

namespace levk {
Serializer::Result<Serializable> Serializer::deserialize(dj::Json const& json) const {
	auto const type_name = std::string{json["type_name"].as_string()};
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
	return {std::move(ret), entry.type_id, entry.is_component};
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

TypeId Serializer::type_id(std::string const& type_name) const {
	if (auto it = m_entries.find(type_name); it != m_entries.end()) { return it->second.type_id; }
	return {};
}

Ptr<Serializer::Entry const> Serializer::find_entry(std::string const& type_name) const {
	if (auto it = m_entries.find(type_name); it != m_entries.end()) { return &it->second; }
	return {};
}

void Serializer::bind_to(std::string&& type_name, TypeId type_id, Factory<Serializable>&& factory, Bool is_component) {
	if (type_name.empty()) {
		logger::warn("[Serializer] Ignoring attempt to bind empty type_name");
		return;
	}
	if (!factory) {
		logger::warn("[Serializer] Ignoring attempt to bind null Factory to type_name [{}]", type_name);
		return;
	}
	if (type_id == TypeId{}) {
		logger::warn("[Serializer] Ignoring attempt to bind null TypeId to type_name [{}]", type_name);
		return;
	}
	m_entries.insert_or_assign(std::move(type_name), Entry{std::move(factory), type_id, is_component.value});
}
} // namespace levk
