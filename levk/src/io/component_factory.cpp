#include <levk/io/component_factory.hpp>
#include <levk/io/serializer.hpp>
#include <levk/service.hpp>
#include <levk/util/logger.hpp>

namespace levk {
bool ComponentFactory::bind_to(std::string type_name, TypeId type_id, Factory<Component> factory) {
	if (!BindingMap<Component>::bind_to(type_name, type_id, factory)) { return false; }
	Service<Serializer>::locate().bind_to(std::move(type_name), type_id, std::move(factory));
	return true;
}
} // namespace levk
