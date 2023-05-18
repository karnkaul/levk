#pragma once
#include <levk/window/action_state.hpp>
#include <levk/window/axis_state.hpp>

namespace levk {
///
/// \brief Set of all Mouse buttons
///
enum class GamepadButton : std::uint32_t {
	eA,
	eB,
	eX,
	eY,
	eLeftBumper,
	eRightBumper,
	eBack,
	eStart,
	eGuide,
	eLeftThumb,
	eRightThumb,
	eDPadUp,
	eDPadRight,
	eDPadDown,
	eDPadLeft,
	eCOUNT_,
};

///
/// \brief Set of all gamepad axes
///
enum class GamepadAxis {
	eLeftX,
	eLeftY,
	eRightX,
	eRightY,
	eLeftTrigger,
	eRightTrigger,
	eCOUNT_,
};

struct Gamepad {
	ActionState<GamepadButton, 16> buttons{};
	AxisState<GamepadAxis, 8> axes{};
	bool is_active{};

	explicit constexpr operator bool() const { return is_active; }
};
} // namespace levk
