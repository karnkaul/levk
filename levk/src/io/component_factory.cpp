#include <levk/io/component_factory.hpp>
#include <levk/io/serializer.hpp>
#include <levk/scene/entity.hpp>
#include <levk/service.hpp>
#include <levk/util/logger.hpp>

namespace levk {
bool ComponentFactory::attach(Entity& out, std::string const& type_name) const {
	auto it = m_entries.find(type_name);
	if (it == m_entries.end()) { return false; }
	auto const& entry = it->second;
	auto component = entry.factory();
	if (!component) { return false; }
	out.attach(entry.type_id.value(), std::move(component));
	return true;
}

bool ComponentFactory::bind_to(std::string type_name, TypeId type_id, Factory<Component> factory) {
	if (!BindingMap<Component>::bind_to(type_name, type_id, factory)) { return false; }
	Service<Serializer>::locate().bind_to(std::move(type_name), type_id, std::move(factory));
	return true;
}
} // namespace levk
