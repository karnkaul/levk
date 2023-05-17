#pragma once
#include <cstdint>

namespace levk {
///
/// \brief Set of all Keys
///
enum class Key : std::uint32_t {
	eUnknown = 0,
	eSpace = 32,
	eApostrophe = 39,
	eComma = 44,
	eMinus = 45,
	eFullstop = 46,
	eSlash = 47,
	e0 = 48,
	e1 = 49,
	e2 = 50,
	e3 = 51,
	e4 = 52,
	e5 = 53,
	e6 = 54,
	e7 = 55,
	e8 = 56,
	e9 = 57,
	eSemicolon = 59,
	eEqual = 61,
	eA = 65,
	eB = 66,
	eC = 67,
	eD = 68,
	eE = 69,
	eF = 70,
	eG = 71,
	eH = 72,
	eI = 73,
	eJ = 74,
	eK = 75,
	eL = 76,
	eM = 77,
	eN = 78,
	eO = 79,
	eP = 80,
	eQ = 81,
	eR = 82,
	eS = 83,
	eT = 84,
	eU = 85,
	eV = 86,
	eW = 87,
	eX = 88,
	eY = 89,
	eZ = 90,
	eLeftBracket = 91,
	eBackslash = 92,
	eRightBracket = 93,
	eGrave = 94,
	eBacktick = eGrave,

	eEscape = 256,
	eEnter = 257,
	eTab = 258,
	eBackspace = 259,
	eInsert = 260,
	eDelete = 261,
	eRight = 262,
	eLeft = 263,
	eDown = 264,
	eUp = 265,
	ePageUp = 266,
	ePageDown = 267,
	eHome = 268,
	eEnd = 269,
	eCapsLock = 280,
	eScrollLock = 281,
	eNumLock = 282,
	ePrintScreen = 283,
	ePause = 284,
	eF1 = 290,
	eF2 = 291,
	eF3 = 292,
	eF4 = 293,
	eF5 = 294,
	eF6 = 295,
	eF7 = 296,
	eF8 = 297,
	eF9 = 298,
	eF10 = 299,
	eF11 = 300,
	eF12 = 301,
	eF13 = 302,
	eF14 = 303,
	eF15 = 304,
	eF16 = 305,
	eF17 = 306,
	eF18 = 307,
	eF19 = 308,
	eF20 = 309,
	eF21 = 310,
	eF22 = 311,
	eF23 = 312,
	eF24 = 313,
	eF25 = 314,
	eKp0 = 320,
	eKp1 = 321,
	eKp2 = 322,
	eKp3 = 323,
	eKp4 = 324,
	eKp5 = 325,
	eKp6 = 326,
	eKp7 = 327,
	eKp8 = 328,
	eKp9 = 329,
	eKpDecimal = 330,
	eKpDivide = 331,
	eKpMultiply = 332,
	eKpSubtract = 333,
	eKpAdd = 334,
	eKpEnter = 335,
	eKpEqual = 336,
	eLeftShift = 340,
	eLeftControl = 341,
	eLeftAlt = 342,
	eLeftSuper = 343,
	eRightShift = 344,
	eRightControl = 345,
	eRightAlt = 346,
	eRightSuper = 347,
	eMenu = 348,
	eCOUNT_,
};

///
/// \brief Set of all Mouse buttons
///
enum class MouseButton : std::uint32_t {
	e1 = 0,
	e2,
	e3,
	e4,
	e5,
	e6,
	e7,
	e8,
	eCOUNT_,

	eLast = e8,
	eLeft = e1,
	eRight = e2,
	eMiddle = e3,
};

///
/// \brief Set of all Actions
///
enum class Action : std::uint8_t { eNone, eRelease, ePress, eHold, eRepeat };

///
/// \brief Bit flag representing a Key / MouseButton Modifier.
///
enum class Modifier : std::uint8_t {
	eNone = 0,
	eShift = 1 << 0,
	eCtrl = 1 << 1,
	eAlt = 1 << 2,
	eSuper = 1 << 3,
	eCapsLock = 1 << 4,
	eNumLock = 1 << 5,
};

///
/// \brief Bit flags for Modifiers.
///
using Modifiers = std::uint32_t;

///
/// \brief Mouse cursor mode.
///
enum class CursorMode { eNormal, eDisabled, eHidden };
} // namespace levk
