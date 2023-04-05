#include <glm/gtx/matrix_decompose.hpp>
#include <levk/transform.hpp>

namespace levk {
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
