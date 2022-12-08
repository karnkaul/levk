#pragma once
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace levk {
///
/// \brief 3-channel colour with HDR support.
///
struct Rgb {
	///
	/// \brief 8-bit channels.
	///
	glm::tvec3<std::uint8_t> channels{0xff, 0xff, 0xff};
	///
	/// \brief Intensity (> 1.0 == HDR).
	///
	float intensity{1.0f};

	///
	/// \brief Convert an 8-bit channels to a normalized float.
	/// \param channel 8-bit channel
	/// \returns Normalized float
	///
	static constexpr float to_f32(std::uint8_t channel) { return static_cast<float>(channel) / static_cast<float>(0xff); }
	///
	/// \brief Convert a normalized float into an 8-bit channel.
	/// \param normalized Normalized float
	/// \returns 8-bit channel
	///
	static constexpr std::uint8_t to_u8(float normalized) { return static_cast<std::uint8_t>(normalized * static_cast<float>(0xff)); }
	///
	/// \brief Convert linear input to sRGB.
	/// \param linear 4 normalized floats encoded as linear
	/// \returns 4 normalized floats encoded as sRGB
	///
	static glm::vec4 to_srgb(glm::vec4 const& linear);
	///
	/// \brief Convert sRGB input to linear.
	/// \param srgb 4 normalized floats encoded as sRGB
	/// \returns 4 normalized floats encoded as linear
	///
	static glm::vec4 to_linear(glm::vec4 const& srgb);

	///
	/// \brief Construct an Rgb instance using normalized input and intensity.
	/// \param normalized 3 channels of normalized [0-1] floats
	/// \param intensity Intensity of returned Rgb
	/// \returns Rgb instance
	///
	static constexpr Rgb make(glm::vec3 const& normalized, float intensity = 1.0f) {
		return {
			.channels = {to_u8(normalized.x), to_u8(normalized.y), to_u8(normalized.z)},
			.intensity = intensity,
		};
	}

	///
	/// \brief Obtain only the normalzed tint (no HDR).
	///
	constexpr glm::vec4 to_tint(float alpha) const { return glm::vec4{to_f32(channels.x), to_f32(channels.y), to_f32(channels.z), alpha}; }
	///
	/// \brief Convert instance to 3 channel normalized output.
	/// \returns 3 normalized floats
	///
	constexpr glm::vec3 to_vec3() const { return to_vec4(1.0f); }
	///
	/// \brief Convert instance to 4 channel normalized output.
	/// \returns 4 normalized floats
	///
	constexpr glm::vec4 to_vec4(float alpha = 1.0f) const { return intensity * to_tint(alpha); }
};
} // namespace levk
