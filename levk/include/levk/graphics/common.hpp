#pragma once
#include <glm/vec2.hpp>
#include <levk/camera.hpp>
#include <levk/graphics/primitive.hpp>
#include <levk/lights.hpp>
#include <levk/rgba.hpp>
#include <levk/util/dyn_array.hpp>
#include <levk/util/ptr.hpp>

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

struct AntiAliasing {
	enum Type : std::uint8_t {
		e1x = 1 << 0,
		e2x = 1 << 1,
		e4x = 1 << 2,
		e8x = 1 << 3,
		e16x = 1 << 4,
		e32x = 1 << 5,
	};

	std::uint8_t flags{};
};

struct Vsync {
	enum Type : std::uint8_t {
		eOn = 1 << 0,
		eAdaptive = 1 << 1,
		eOff = 1 << 2,
		eMailbox = 1 << 3,
	};

	std::uint8_t flags{};
};

struct ShaderCode {
	DynArray<std::uint32_t> spir_v{};
	std::size_t hash{};
};

struct GraphicsDeviceInfo {
	std::string_view name{};
	bool validation{};
	bool portability{};
	ColourSpace swapchain{};
	Vsync supported_vsync{};
	Vsync::Type current_vsync{};
	AntiAliasing supported_aa{};
	AntiAliasing::Type current_aa{};
	float render_scale{1.0f};
};

struct GraphicsDeviceCreateInfo {
	Window const& window;
	bool validation{true};
	ColourSpace swapchain{ColourSpace::eSrgb};
	Vsync::Type vsync{Vsync::eAdaptive};
	AntiAliasing::Type anti_aliasing{AntiAliasing::e2x};
};

struct RenderStats {
	std::uint64_t draw_calls{};
};
} // namespace levk
