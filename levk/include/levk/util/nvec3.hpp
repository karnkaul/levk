#pragma once
#include <glm/gtx/norm.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/vec3.hpp>

namespace levk {
inline constexpr auto right_v = glm::vec3{1.0f, 0.0f, 0.0f};
inline constexpr auto up_v = glm::vec3{0.0f, 1.0f, 0.0f};
inline constexpr auto front_v = glm::vec3{0.0f, 0.0f, 1.0f};

///
/// \brief Normalized 3D vector (enforces unit length).
///
class nvec3 {
  public:
	nvec3() = default;
	///
	/// \brief Construct a normalized vec3.
	/// \param dir The direction to normalize
	///
	nvec3(glm::vec3 dir) : m_value{glm::normalize(dir)} {}

	///
	/// \brief Obtain the underlying vec3.
	/// \returns The underlying vec3
	///
	constexpr glm::vec3 const& value() const { return m_value; }
	constexpr operator glm::vec3 const&() const { return value(); }

	///
	/// \brief Rotate the direction vector.
	/// \param rad Radians to rotate by
	/// \param axis Axis to rotate around
	/// \returns Reference to itself
	///
	nvec3& rotate(float rad, glm::vec3 axis) {
		m_value = glm::normalize(glm::rotate(m_value, rad, axis));
		return *this;
	}

	///
	/// \brief Obtain a rotated direction vector.
	/// \param rad Radians to rotate by
	/// \param axis Axis to rotate around
	/// \returns New instance of rotated unit vector
	///
	nvec3 rotated(float rad, glm::vec3 axis) const {
		auto ret = *this;
		ret.rotate(rad, axis);
		return ret;
	}

  private:
	glm::vec3 m_value{front_v};
};
} // namespace levk
