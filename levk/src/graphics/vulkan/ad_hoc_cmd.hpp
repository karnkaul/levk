#pragma once
#include <graphics/vulkan/device.hpp>

namespace levk::vulkan {
class AdHocCmd {
  public:
	AdHocCmd(Device::View const& device, vk::PipelineStageFlags wait = vk::PipelineStageFlagBits::eAllCommands)
		: m_device(device), m_allocator(device.make_command_allocator()), m_wait(wait), m_fence(device.device.createFenceUnique({})) {
		cb = m_allocator.allocate();
		cb.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
	}

	~AdHocCmd() {
		cb.end();
		auto si = vk::SubmitInfo{};
		si.pWaitDstStageMask = &m_wait;
		si.commandBufferCount = 1u;
		si.pCommandBuffers = &cb;
		auto result = m_device.queue->with([&](vk::Queue queue) { return queue.submit(1u, &si, *m_fence); });
		if (result != vk::Result::eSuccess) { return; }
		m_device.wait(*m_fence);
	}

	AdHocCmd& operator=(AdHocCmd&&) = delete;

	vk::CommandBuffer cb{};
	std::vector<UniqueBuffer> scratch_buffers{};

  private:
	Device::View m_device;
	CommandAllocator m_allocator{};
	vk::PipelineStageFlags m_wait{};
	vk::UniqueFence m_fence{};
};
} // namespace levk::vulkan
