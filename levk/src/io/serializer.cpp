#include <levk/io/serializer.hpp>
#include <levk/scene/entity.hpp>
#include <levk/util/logger.hpp>

namespace levk {
namespace {
auto const g_log{Logger{"Serializer"}};
}

bool Serializer::bind_to(std::string type_name, TypeId type_id, Factory<Serializable> factory, Tags tags) {
	if (type_name.empty()) {
		g_log.warn("Ignoring attempt to bind empty type_name");
		return false;
	}
	if (!factory) {
		g_log.warn("Ignoring attempt to bind null Factory to type_name [{}]", type_name);
		return false;
	}
	if (type_id == TypeId{}) {
		g_log.warn("Ignoring attempt to bind null TypeId to type_name [{}]", type_name);
		return false;
	}
	m_entries.insert_or_assign(type_name, Entry{std::move(factory), type_id, tags});
	m_type_names.insert(type_name);
	return true;
}

TypeId Serializer::type_id(std::string const& type_name) const {
	if (auto it = m_entries.find(type_name); it != m_entries.end()) { return it->second.type_id; }
	return {};
}

std::vector<std::string_view> Serializer::type_names_by_tag(Tag tag) const {
	auto ret = std::vector<std::string_view>{};
	for (auto const& [type_name, entry] : m_entries) {
		if (entry.tags.test(tag)) { ret.push_back(type_name); }
	}
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

bool Serializer::attach(Entity& out, std::string const& type_name) const {
	auto it = m_entries.find(type_name);
	if (it == m_entries.end()) { return false; }
	auto const& entry = it->second;
	auto component = dynamic_unique_cast<Component>(entry.factory());
	if (!component) { return false; }
	out.attach(entry.type_id.value(), std::move(component));
	return true;
}
} // namespace levk
