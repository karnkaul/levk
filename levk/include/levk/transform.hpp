#pragma once
#include <glm/gtx/quaternion.hpp>

namespace levk {
///
/// \brief Identity quaternion.
///
inline constexpr auto quat_identity_v = glm::identity<glm::quat>();
///
/// \brief Identity matrix.
///
inline constexpr auto matrix_identity_v = glm::identity<glm::mat4x4>();

///
/// \brief Models an affine transformation in 3D space.
///
/// Provides friendly APIs for positioning, rotation, and scaling.
/// Caches combined 4x4 transformation matrix to avoid recomputing if unchanged.
///
class Transform {
  public:
	///
	/// \brief The front-end data representing a transformation.
	///
	struct Data {
		glm::vec3 position{};
		glm::quat orientation{quat_identity_v};
		glm::vec3 scale{1.0f};
	};

	///
	/// \brief Obtain the transform data.
	/// \returns The transform data
	///
	Data const& data() const { return m_data; }
	///
	/// \brief Obtain the position.
	/// \returns The position
	///
	glm::vec3 position() const { return m_data.position; }
	///
	/// \brief Obtain the orientation.
	/// \returns The orientation
	///
	glm::quat orientation() const { return m_data.orientation; }
	///
	/// \brief Obtain the scale.
	/// \returns The scale
	///
	glm::vec3 scale() const { return m_data.scale; }

	///
	/// \brief Set the transform data.
	/// \param data The data to set to
	/// \returns Reference to self
	///
	Transform& set_data(Data data);
	///
	/// \brief Set the position.
	/// \param position The position to set to
	/// \returns Reference to self
	///
	Transform& set_position(glm::vec3 position);
	///
	/// \brief Set the orientation.
	/// \param orientation The orientation to set to
	/// \returns Reference to self
	///
	Transform& set_orientation(glm::quat orientation);
	///
	/// \brief Set the scale.
	/// \param scale The scale to set to
	/// \returns Reference to self
	///
	Transform& set_scale(glm::vec3 scale);

	///
	/// \brief Rotate the transform using an angle-axis approach.
	///
	///
	Transform& rotate(float radians, glm::vec3 const& axis);
	///
	/// \brief Decompose the given matrix into the corresponding position, orientation, and scale.
	/// \param mat Matrix to decompose
	/// \returns Self
	///
	Transform& decompose(glm::mat4 const& mat);

	///
	/// \brief Obtain the combined 4x4 transformation matrix.
	/// \returns The transformation matrix
	///
	glm::mat4 const& matrix() const;
	///
	/// \brief Recompute the transformation matrix.
	///
	void recompute() const;
	///
	/// \brief Check if the transformation matrix is stale / out of date.
	/// \returns true If the matrix needs to be recomputed
	///
	bool is_dirty() const { return m_dirty; }

  private:
	Transform& set_dirty();

	mutable glm::mat4x4 m_matrix{matrix_identity_v};
	Data m_data{};
	mutable bool m_dirty{};
};

// impl

inline Transform& Transform::set_data(Data data) {
	m_data = data;
	return set_dirty();
}

inline Transform& Transform::set_position(glm::vec3 position) {
	m_data.position = position;
	return set_dirty();
}

inline Transform& Transform::set_orientation(glm::quat orientation) {
	m_data.orientation = glm::normalize(orientation);
	return set_dirty();
}

inline Transform& Transform::set_scale(glm::vec3 scale) {
	m_data.scale = scale;
	return set_dirty();
}

inline Transform& Transform::rotate(float radians, glm::vec3 const& axis) {
	m_data.orientation = glm::rotate(m_data.orientation, radians, axis);
	return set_dirty();
}

inline glm::mat4 const& Transform::matrix() const {
	if (is_dirty()) { recompute(); }
	return m_matrix;
}

inline void Transform::recompute() const {
	m_matrix = glm::translate(matrix_identity_v, m_data.position) * glm::toMat4(m_data.orientation) * glm::scale(matrix_identity_v, m_data.scale);
	m_dirty = false;
}

inline Transform& Transform::set_dirty() {
	m_dirty = true;
	return *this;
}
} // namespace levk
