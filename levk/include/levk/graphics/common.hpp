#pragma once
#include <glm/vec2.hpp>
#include <levk/util/bit_flags.hpp>
#include <levk/util/ptr.hpp>
#include <string_view>

namespace levk {
class Window;

using Extent2D = glm::uvec2;

enum class ColourSpace : std::uint8_t { eSrgb, eLinear };
enum class MeshType : std::uint8_t { eNone, eStatic, eSkinned };

struct RenderMode {
	enum class Type : std::uint8_t { eDefault, eFill, eLine, ePoint };

	float line_width{1.0f};
	Type type{Type::eDefault};
	bool depth_test{true};

	static constexpr RenderMode wireframe(float line_width = 1.0f, bool depth_test = true) { return {line_width, Type::eLine, depth_test}; }

	bool operator==(RenderMode const&) const = default;
};

inline constexpr std::size_t max_sets_v{16};
inline constexpr std::size_t max_bindings_v{16};
inline constexpr std::size_t max_lights_v{4};
inline constexpr float render_scale_limit_v[] = {0.2f, 8.0f};
inline constexpr Extent2D shadow_resolution_limit_v[] = {{256, 256}, {8192, 8192}};
inline constexpr glm::vec2 shadow_frustum_limit_v[] = {{1.0f, 1.0f}, {1024.0f, 1024.0f}};

enum class AntiAliasing : std::uint8_t {
	e1x = 1 << 0,
	e2x = 1 << 1,
	e4x = 1 << 2,
	e8x = 1 << 3,
	e16x = 1 << 4,
	e32x = 1 << 5,
};

using AntiAliasingFlags = BitFlags<AntiAliasing>;

enum class Vsync : std::uint8_t {
	eOn = 1 << 0,
	eAdaptive = 1 << 1,
	eOff = 1 << 2,
	eMailbox = 1 << 3,
};

using VsyncFlags = BitFlags<Vsync>;
} // namespace levk
