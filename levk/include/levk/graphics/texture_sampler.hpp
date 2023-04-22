#pragma once
#include <cstdint>

namespace levk {
struct TextureSampler {
	enum class Wrap : std::uint8_t { eRepeat, eClampEdge, eClampBorder };
	enum class Filter : std::uint8_t { eLinear, eNearest };
	enum class Border : std::uint8_t { eOpaqueBlack, eOpaqueWhite, eTransparentBlack };

	Wrap wrap_s{Wrap::eRepeat};
	Wrap wrap_t{Wrap::eRepeat};
	Filter min{Filter::eLinear};
	Filter mag{Filter::eLinear};
	Border border{Border::eOpaqueBlack};

	bool operator==(TextureSampler const&) const = default;
};
} // namespace levk
