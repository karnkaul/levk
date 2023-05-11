#pragma once
#include <graphics/vulkan/common.hpp>

namespace levk::vulkan {
struct Texture {
	DeviceView device{};
	TextureSampler sampler{};
	ImageCreateInfo create_info{};
	Defer<UniqueImage> image{};
};
} // namespace levk::vulkan
