#pragma once
#include <levk/util/enum_array.hpp>
#include <levk/util/time.hpp>

namespace levk {
struct FrameProfile {
	enum class Type {
		eTick,
		eAcquireFrame,
		eRender3D,
		eRenderUI,
		eRenderSubmit,
		eRenderPresent,

		eFrameTime,
		eCOUNT_,
	};

	static constexpr EnumArray<Type, std::string_view> to_string_v{
		"tick", "acquire-frame", "render-3d", "render-ui", "render-submit", "render-present", "frame-time",
	};

	EnumArray<Type, Time> profile{};
};
} // namespace levk
