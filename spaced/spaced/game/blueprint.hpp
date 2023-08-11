#pragma once
#include <levk/scene/entity.hpp>

namespace spaced {
struct Blueprint {
	std::uint32_t created{};

	virtual ~Blueprint() = default;

	virtual std::string_view entity_name() const = 0;
	virtual void create(levk::Entity& out);

	std::string indexed_name() const;
};
} // namespace spaced
