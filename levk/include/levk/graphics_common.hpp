#pragma once
#include <glm/vec2.hpp>
#include <levk/camera.hpp>
#include <levk/lights.hpp>
#include <levk/node.hpp>
#include <levk/rgba.hpp>
#include <cstdint>
#include <span>

namespace levk {
class Window;
struct StaticMesh;
struct SkinnedMesh;
struct SkeletonInstance;
struct MeshResources;

using Extent2D = glm::uvec2;

enum class ColourSpace : std::uint8_t { eSrgb, eLinear };
enum class PolygonMode : std::uint8_t { eFill, eLine, ePoint };
enum class Topology : std::uint8_t { ePointList, eLineList, eLineStrip, eTriangleList, eTriangleStrip, eTriangleFan };

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

struct PipelineState {
	PolygonMode polygon_mode{PolygonMode::eFill};
	Topology topology{Topology::eTriangleList};
	float line_width{1.0f};
	bool depth_test{true};

	bool operator==(PipelineState const&) const = default;
};

class GraphicsDevice;

struct GraphicsRenderer {
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
};

struct StaticMeshRenderInfo {
	MeshResources const& resources;
	StaticMesh const& mesh;
	glm::mat4 const& parent;
	std::span<Transform const> instances;
};

struct SkinnedMeshRenderInfo {
	MeshResources const& resources;
	SkinnedMesh const& mesh;
	SkeletonInstance const& skeleton;
	Node::Tree const& tree;
};
} // namespace levk
