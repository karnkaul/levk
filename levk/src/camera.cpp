#include <levk/camera.hpp>
#include <levk/util/visitor.hpp>

namespace levk {
glm::mat4 Camera::view() const {
	auto const centre = transform.orientation() * -front_v;
	auto const up = transform.orientation() * up_v;
	return glm::lookAt(transform.position(), transform.position() + centre, up);
}

glm::mat4 Camera::projection(glm::vec2 extent) const {
	if (extent.x == 0.0f || extent.y == 0.0f) { return glm::identity<glm::mat4>(); }
	auto const visitor = Visitor{
		[extent](Perspective const& p) { return glm::perspective(p.field_of_view, extent.x / extent.y, p.view_plane.near, p.view_plane.far); },
		[extent](Orthographic const& o) {
			auto const he = 0.5f * extent;
			return glm::ortho(-he.x, he.x, -he.y, he.y, o.view_plane.near, o.view_plane.far);
		},
	};
	return std::visit(visitor, type);
}
} // namespace levk
