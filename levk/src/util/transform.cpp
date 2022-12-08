#include <levk/util/transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>

namespace levk {
Transform& Transform::set_matrix(glm::mat4 mat) {
	auto skew = glm::vec3{};
	auto persp = glm::vec4{};
	glm::decompose(mat, m_data.scale, m_data.orientation, m_data.position, skew, persp);
	m_matrix = mat;
	m_dirty = false;
	return *this;
}
} // namespace levk
