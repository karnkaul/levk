#include <glm/gtx/matrix_decompose.hpp>
#include <levk/transform.hpp>

namespace levk {
glm::quat Transform::look_at(glm::vec3 const& point, glm::vec3 const& eye, glm::quat const& start, nvec3 const& up) {
	glm::vec3 const current_look = start * front_v;
	auto const to_point = point - eye;
	auto const dot = glm::dot(current_look, glm::normalize(to_point));
	if (1.0f - dot < 0.001f) { return glm::identity<glm::quat>(); }
	if (1.0f + dot < 0.001f) { return glm::angleAxis(glm::radians(180.0f), up.value()); }
	auto const axis = glm::normalize(glm::cross(current_look, to_point));
	return glm::angleAxis(glm::acos(dot), axis);
}

Transform& Transform::decompose(glm::mat4 const& mat) {
	auto skew = glm::vec3{};
	auto persp = glm::vec4{};
	glm::decompose(mat, m_data.scale, m_data.orientation, m_data.position, skew, persp);
	m_matrix = mat;
	m_dirty = false;
	return *this;
}

Transform Transform::combined(glm::mat4 const& parent) const {
	auto ret = Transform{};
	ret.decompose(parent * matrix());
	return ret;
}
} // namespace levk
