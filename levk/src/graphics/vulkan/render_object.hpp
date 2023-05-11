#pragma once
#include <graphics/vulkan/common.hpp>
#include <levk/graphics/draw_list.hpp>
#include <optional>

namespace levk::vulkan {
struct RenderObject {
	struct Instances {
		static constexpr std::uint32_t vertex_binding_v{4u};

		BufferView mats_vbo{};
		std::uint32_t count{1u};
	};

	struct Joints {
		static constexpr std::uint32_t vertex_binding_v{8u};
		static constexpr std::uint32_t descriptor_set_v{3u};
		static constexpr std::uint32_t descriptor_binding_v{0u};

		BufferView mats_ssbo{};
	};

	struct Shadow {
		static constexpr std::uint32_t descriptor_binding_v{1u};
	};

	static std::vector<RenderObject> build_objects(DrawList const& draw_list, HostBuffer::Pool& buffer_pool);
	static RenderObject build(Drawable drawable, Primitive const& primitive, HostBuffer::Pool& buffer_pool, std::span<BufferView const> joints_mats);

	Drawable drawable;
	Instances instances{};
	Joints joints{};
};
} // namespace levk::vulkan
