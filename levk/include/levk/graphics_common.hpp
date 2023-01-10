#pragma once
#include <glm/vec2.hpp>
#include <levk/camera.hpp>
#include <levk/lights.hpp>
#include <levk/rgba.hpp>
#include <levk/util/ptr.hpp>
#include <cstdint>
#include <span>

namespace levk {
class Window;
struct StaticMesh;
struct SkinnedMesh;
struct RenderResources;

using Extent2D = glm::uvec2;

enum class ColourSpace : std::uint8_t { eSrgb, eLinear };
enum class Topology : std::uint8_t { ePointList, eLineList, eLineStrip, eTriangleList, eTriangleStrip, eTriangleFan };
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

struct GraphicsDeviceInfo {
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
	Vsync::Type vsync{Vsync::eOn};
	AntiAliasing::Type anti_aliasing{AntiAliasing::e2x};
};

class GraphicsDevice;

struct GraphicsRenderer {
	Ptr<GraphicsRenderer> next_renderer{};

	virtual ~GraphicsRenderer() = default;

	virtual void render_3d() = 0;
	virtual void render_ui() = 0;
};

struct RenderInfo {
	GraphicsRenderer& renderer;
	Camera const& camera;
	Lights const& lights;
	Extent2D extent;
	Rgba clear;
	RenderMode default_render_mode;
};

struct StaticMeshRenderInfo {
	RenderResources const& resources;
	StaticMesh const& mesh;
	glm::mat4 const& parent;
	std::span<Transform const> instances;
};

struct SkinnedMeshRenderInfo {
	RenderResources const& resources;
	SkinnedMesh const& mesh;
	std::span<glm::mat4 const> joints;
};
} // namespace levk

namespace levk::refactor {
struct StaticMesh;
struct SkinnedMesh;
struct RenderResources;

struct StaticMeshRenderInfo {
	RenderResources const& resources;
	StaticMesh const& mesh;
	glm::mat4 const& parent;
	std::span<Transform const> instances;
};

struct SkinnedMeshRenderInfo {
	RenderResources const& resources;
	SkinnedMesh const& mesh;
	std::span<glm::mat4 const> joints;
};
} // namespace levk::refactor
