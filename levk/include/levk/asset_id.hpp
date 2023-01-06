#pragma once
#include <levk/asset/uri.hpp>
#include <levk/util/id.hpp>

namespace levk {
template <typename Type>
struct AssetId {
	asset::Uri<Type> uri{};
	Id<Type> id{};

	bool valid() const { return id != Id<Type>{}; }
	explicit operator bool() const { return valid(); }
};
} // namespace levk
