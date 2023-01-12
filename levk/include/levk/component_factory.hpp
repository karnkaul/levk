#pragma once
#include <levk/component.hpp>
#include <levk/util/binding_map.hpp>

namespace levk {
class ComponentFactory : public BindingMap<Component> {
  public:
  private:
	bool bind_to(std::string type_name, TypeId type_id, Factory<Component> factory) override;
};
} // namespace levk
