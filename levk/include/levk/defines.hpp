#pragma once

namespace levk {
constexpr bool debug_v =
#if defined(LEVK_DEBUG)
	true;
#else
	false;
#endif
} // namespace levk
