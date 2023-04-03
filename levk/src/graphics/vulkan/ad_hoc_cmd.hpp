#pragma once
#include <graphics/vulkan/device.hpp>

namespace levk::vulkan {
class AdHocCmd {
  public:
	AdHocCmd(Device::View const& device) : m_device(device), m_allocator(device.make_command_allocator()), m_fence(device.device.createFenceUnique({})) {
		cb = m_allocator.allocate();
		cb.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
	}

	~AdHocCmd() {
		cb.end();
		auto si = vk::SubmitInfo2{};
		auto cbi = vk::CommandBufferSubmitInfo{cb};
		si.commandBufferInfoCount = 1u;
		si.pCommandBufferInfos = &cbi;
		auto result = m_device.queue->with([&](vk::Queue queue) { return queue.submit2(1u, &si, *m_fence); });
		if (result != vk::Result::eSuccess) { return; }
		m_device.wait(*m_fence);
	}

	AdHocCmd& operator=(AdHocCmd&&) = delete;

	vk::CommandBuffer cb{};
	std::vector<UniqueBuffer> scratch_buffers{};

  private:
	Device::View m_device;
	CommandAllocator m_allocator{};
	vk::UniqueFence m_fence{};
};
} // namespace levk::vulkan
