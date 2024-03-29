#pragma once
#include <glm/gtc/quaternion.hpp>
#include <levk/aabb.hpp>
#include <levk/graphics/camera.hpp>
#include <levk/graphics/rgba.hpp>
#include <levk/imcpp/common.hpp>
#include <levk/rect.hpp>
#include <levk/transform.hpp>
#include <levk/util/nvec3.hpp>
#include <levk/util/radians.hpp>

namespace levk::imcpp {
///
/// \brief Stateless ImGui helper to reflect various properties.
///
class Reflector {
  public:
	struct AsRgb {
		glm::vec3& out;
	};

	static constexpr auto min_v{-std::numeric_limits<float>::max()};
	static constexpr auto max_v{std::numeric_limits<float>::max()};

	///
	/// \brief Construct an Reflector instance.
	///
	/// Reflectors don't do anything on construction, constructors exist to enforce invariants instance-wide.
	/// For all Reflectors, an existing Window target is required, Reflector instances will not create any
	///
	Reflector(NotClosed<Window>) {}

	bool operator()(char const* label, glm::vec2& out_vec2, float speed = 1.0f, float lo = min_v, float hi = max_v) const;
	bool operator()(char const* label, glm::vec3& out_vec3, float speed = 1.0f, float lo = min_v, float hi = max_v) const;
	bool operator()(char const* label, glm::vec4& out_vec4, float speed = 1.0f, float lo = min_v, float hi = max_v) const;
	bool operator()(char const* label, nvec3& out_vec3, float speed = 0.01f) const;
	bool operator()(char const* label, glm::quat& out_quat) const;
	bool operator()(char const* label, Radians& out_as_degrees, float speed = 0.05f, float lo = min_v, float hi = max_v) const;
	bool operator()(char const* label, AsRgb out_rgb) const;
	bool operator()(Rgba& out_rgba, Bool show_alpha) const;
	bool operator()(HdrRgba& out_rgba, Bool show_alpha, Bool show_intensity = {true}) const;
	bool operator()(Transform& out_transform, Bool& out_unified_scaling, Bool scaling_toggle) const;
	bool operator()(ViewPlane& view_plane) const;
	bool operator()(Camera::Orthographic& orthographic) const;
	bool operator()(Camera::Perspective& perspective) const;
	bool operator()(Camera& out_camera) const;
	bool operator()(UvRect& out_uv) const;
	bool operator()(Aabb& out_aabb) const;
};
} // namespace levk::imcpp
