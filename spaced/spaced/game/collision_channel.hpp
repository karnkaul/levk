#pragma once
#include <cstdint>

namespace spaced {
enum CollisionChannel : std::uint32_t {
	eCC_Player = 1 << 0,
	eCC_Hostile = 1 << 1,
};
}
