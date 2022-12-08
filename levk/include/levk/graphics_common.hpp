#pragma once
#include <cstdint>

namespace levk {
class Window;

enum class ColourSpace : std::uint8_t { eSrgb, eLinear };
enum class PolygonMode : std::uint8_t { eFill, eLine, ePoint };
enum class Topology : std::uint8_t { ePointList, eLineList, eLineStrip, eTriangleList, eTriangleStrip, eTriangleFan };

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

struct GraphicsDeviceInfo {
	bool validation{};
	bool portability{};
	ColourSpace swapchain{};
	Vsync supported_vsync{};
	Vsync::Type current_vsync{};
	AntiAliasing supported_aa{};
	AntiAliasing::Type current_aa{};
};

struct GraphicsDeviceCreateInfo {
	Window const& window;
	bool validation{true};
	ColourSpace swapchain{ColourSpace::eSrgb};
	Vsync::Type vsync{Vsync::eOn};
	AntiAliasing::Type anti_aliasing{AntiAliasing::e2x};
};

struct PipelineState {
	PolygonMode polygon_mode{PolygonMode::eFill};
	Topology topology{Topology::eTriangleList};
	bool depth_test{true};

	bool operator==(PipelineState const&) const = default;
};

struct RenderContext {};

struct GraphicsRenderer {
	virtual ~GraphicsRenderer() = default;

	virtual void render(RenderContext& out) = 0;
};
} // namespace levk
