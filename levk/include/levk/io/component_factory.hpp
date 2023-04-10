#pragma once
#include <levk/io/binding_map.hpp>
#include <levk/scene/component.hpp>

namespace levk {
class ComponentFactory : public BindingMap<Component> {
  public:
	ComponentFactory() { m_logger.context = "ComponentFactory"; }

	bool attach(Entity& out, std::string const& type_name) const;

  private:
	bool bind_to(std::string type_name, TypeId type_id, Factory<Component> factory) override;
};
} // namespace levk
