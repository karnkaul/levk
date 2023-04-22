#pragma once
#include <graphics/vulkan/common.hpp>
#include <levk/graphics/draw_list.hpp>
#include <optional>

namespace levk::vulkan {
struct RenderObject {
	struct List;

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

	Drawable drawable;
	Instances instances{};
	Joints joints{};
};

struct RenderObject::List {
	std::vector<RenderObject> objects{};
	std::vector<BufferView> joints_mats{};

	static List build(DrawList const& in, HostBuffer::Pool& buffer_pool);
};
} // namespace levk::vulkan
