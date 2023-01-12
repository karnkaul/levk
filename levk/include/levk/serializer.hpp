#pragma once
#include <levk/serializable.hpp>
#include <levk/util/binding_map.hpp>

namespace levk {
class Serializer : public BindingMap<Serializable> {
  public:
	dj::Json serialize(Serializable const& serializable) const;
	Result<Serializable> deserialize(dj::Json const& json) const;

	template <std::derived_from<Serializable> To>
	Result<To> deserialize_as(dj::Json const& json) const {
		auto result = deserialize(json);
		return {dynamic_pointer_cast<To>(std::move(result.value)), result.type_id};
	}
};
} // namespace levk
