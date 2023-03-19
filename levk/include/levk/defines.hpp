#pragma once

namespace levk {
constexpr bool debug_v =
#if defined(LEVK_DEBUG)
	true;
#else
	false;
#endif

constexpr bool freetype_v =
#if defined(LEVK_USE_FREETYPE)
	true;
#else
	false;
#endif
} // namespace levk
