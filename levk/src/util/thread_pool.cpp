#include <levk/util/error.hpp>
#include <levk/util/thread_pool.hpp>
#include <algorithm>

namespace levk {
ThreadPool::ThreadPool(std::optional<std::uint32_t> const thread_count) {
	auto const hc = std::thread::hardware_concurrency();
	auto const count = std::min(thread_count.value_or(hc), hc);
	if (count == 0) { return; }

	auto agent = [queue = &m_queue](std::stop_token const& stop) {
		while (auto task = queue->pop(stop)) { (*task)(); }
	};
	for (std::uint32_t i = 0; i < count; ++i) { m_threads.push_back(std::jthread{agent}); }
}

ThreadPool::~ThreadPool() {
	for (auto& thread : m_threads) { thread.request_stop(); }
	m_queue.release();
}
} // namespace levk
