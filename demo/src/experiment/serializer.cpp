#include <experiment/serializer.hpp>
#include <levk/util/logger.hpp>

namespace levk::experiment {
void Serializer::bind_to(std::string type_name, Factory factory) {
	if (type_name.empty()) {
		logger::warn("[Serializer] Ignoring attempt bind empty type_name");
		return;
	}
	if (!factory) {
		logger::warn("[Serializer] Ignoring attempt bind null factory to type_name [{}]", type_name);
		return;
	}
	m_factories.insert_or_assign(std::move(type_name), std::move(factory));
}

std::unique_ptr<ISerializable> Serializer::deserialize(dj::Json const& json) const {
	auto const type_name = std::string{json["type_name"].as_string()};
	if (type_name.empty()) {
		logger::warn("[Serializer] Missing type_name in JSON");
		return {};
	}
	auto it = m_factories.find(type_name);
	if (it == m_factories.end()) {
		logger::warn("[Serializer] Unrecognized type_name in JSON: [{}]", type_name);
		return {};
	}
	auto ret = it->second();
	if (!ret->deserialize(json)) {
		logger::warn("[Serializer] Failed to deserialize [{}]", type_name);
		return {};
	}
	return ret;
}

dj::Json Serializer::serialize(ISerializable const& serializable) const {
	auto ret = dj::Json{};
	auto const type_name = serializable.type_name();
	ret["type_name"] = type_name;
	if (!serializable.serialize(ret)) {
		logger::warn("[Serializer] Failed to serialize [{}]", type_name);
		return {};
	}
	return ret;
}
} // namespace levk::experiment
