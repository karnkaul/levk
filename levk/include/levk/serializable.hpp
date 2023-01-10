#pragma once
#include <djson/json.hpp>

namespace levk {
struct Serializable {
	virtual ~Serializable() = default;
	///
	/// \brief Type name for this type.
	/// \returns Type name for this type
	///
	/// All instances of a concrete type must return identical values for this call.
	/// All type names in a program must be unique.
	///
	virtual std::string_view type_name() const = 0;
	///
	/// \brief Serialize this instance.
	/// \param out JSON object to serialize into
	/// \returns true if successful
	///
	virtual bool serialize(dj::Json& out) const = 0;
	///
	/// \brief Deserialize a JSON into this instance.
	/// \param json JSON object to deserialize from
	/// \returns true if successful
	///
	virtual bool deserialize(dj::Json const& json) = 0;
};
} // namespace levk
