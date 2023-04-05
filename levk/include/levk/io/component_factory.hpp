#pragma once
#include <levk/io/binding_map.hpp>
#include <levk/scene/component.hpp>

namespace levk {
class ComponentFactory : public BindingMap<Component> {
  public:
  private:
	bool bind_to(std::string type_name, TypeId type_id, Factory<Component> factory) override;
};
} // namespace levk
