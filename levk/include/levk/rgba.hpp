#pragma once
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace levk {
///
/// \brief 4-channel colour with HDR support.
///
struct Rgba {
	///
	/// \brief 8-bit channels.
	///
	glm::tvec4<std::uint8_t> channels{0xff, 0xff, 0xff, 0xff};
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
	static constexpr Rgba from(glm::vec4 const& normalized, float intensity = 1.0f) {
		return {
			.channels = {to_u8(normalized.x), to_u8(normalized.y), to_u8(normalized.z), to_u8(normalized.w)},
			.intensity = intensity,
		};
	}

	///
	/// \brief Obtain only the normalzed tint (no HDR).
	///
	constexpr glm::vec4 to_tint() const { return glm::vec4{to_f32(channels.x), to_f32(channels.y), to_f32(channels.z), to_f32(channels.w)}; }

	///
	/// \brief Convert instance to 4 channel normalized output.
	/// \returns 4 normalized floats
	///
	constexpr glm::vec4 to_vec4() const { return glm::vec4{intensity * glm::vec3{to_tint()}, to_f32(channels.w)}; }
};

constexpr auto white_v = Rgba{{0xff, 0xff, 0xff, 0xff}};
constexpr auto black_v = Rgba{{0x0, 0x0, 0x0, 0xff}};
constexpr auto red_v = Rgba{{0xff, 0x0, 0x0, 0xff}};
constexpr auto green_v = Rgba{{0x0, 0xff, 0x0, 0xff}};
constexpr auto blue_v = Rgba{{0x0, 0x0, 0xff, 0xff}};
constexpr auto yellow_v = Rgba{{0xff, 0xff, 0x0, 0xff}};
constexpr auto magenta_v = Rgba{{0xff, 0x0, 0xff, 0xff}};
constexpr auto cyan_v = Rgba{{0x0, 0xff, 0xff, 0xff}};
} // namespace levk
