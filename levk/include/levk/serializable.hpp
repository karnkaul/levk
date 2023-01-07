#pragma once
#include <djson/json.hpp>

namespace levk {
struct ISerializable {
	virtual std::string_view type_name() const = 0;
	virtual bool serialize(dj::Json& out) const = 0;
	virtual bool deserialize(dj::Json const& json) = 0;
};
} // namespace levk
