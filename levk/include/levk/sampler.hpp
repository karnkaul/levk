#pragma once
#include <cstdint>

namespace levk {
struct Sampler {
	enum class Wrap : std::uint8_t { eRepeat, eClampEdge, eClampBorder };
	enum class Filter : std::uint8_t { eLinear, eNearest };

	Wrap wrap_s{Wrap::eRepeat};
	Wrap wrap_t{Wrap::eRepeat};
	Filter min{Filter::eLinear};
	Filter mag{Filter::eLinear};

	bool operator==(Sampler const&) const = default;
};
} // namespace levk
