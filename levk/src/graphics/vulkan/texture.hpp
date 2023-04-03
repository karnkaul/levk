#pragma once
#include <graphics/vulkan/common.hpp>

namespace levk::vulkan {
struct Texture {
	DeviceView device{};
	ImageCreateInfo create_info{};
	Defer<UniqueImage> image{};
};
} // namespace levk::vulkan
