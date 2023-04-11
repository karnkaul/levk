#pragma once
#include <glm/trigonometric.hpp>
#include <cstdint>

namespace levk {
struct Radians;

struct Degrees {
	float value{};

	constexpr Degrees(float value = {}) : value(value) {}
	constexpr Degrees(Radians radians);

	constexpr Radians to_radians() const;

	constexpr operator float() const { return value; }
};

struct Radians {
	float value{};

	constexpr Radians(float value = {}) : value(value) {}
	constexpr Radians(Degrees degrees) : Radians(degrees.to_radians()) {}

	constexpr Degrees to_degrees() const { return {glm::degrees(value)}; }

	constexpr operator float() const { return value; }
};

constexpr Radians Degrees::to_radians() const { return {glm::radians(value)}; }
constexpr Degrees::Degrees(Radians radians) : Degrees(radians.to_degrees()) {}
} // namespace levk
