#include <levk/util/error.hpp>
#include <levk/util/thread_pool.hpp>
#include <algorithm>

namespace levk {
namespace {
struct With {
	std::atomic<std::int32_t>& out;

	With(std::atomic<std::int32_t>& out) : out(out) { ++out; }
	~With() { --out; }

	With& operator=(With&&) = delete;
};
} // namespace

ThreadPool::ThreadPool(std::optional<std::uint32_t> const thread_count) : m_busy(0) {
	auto const hc = std::thread::hardware_concurrency();
	auto const count = std::min(thread_count.value_or(hc), hc);
	if (count == 0) { return; }

	auto agent = [this](std::stop_token const& stop) {
		while (auto task = m_queue.pop(stop)) {
			auto with = With{m_busy};
			(*task)();
		}
	};
	for (std::uint32_t i = 0; i < count; ++i) { m_threads.push_back(std::jthread{agent}); }
}

ThreadPool::~ThreadPool() {
	for (auto& thread : m_threads) { thread.request_stop(); }
	m_queue.release();
}

void ThreadPool::wait_idle() {
	while (!is_idle()) { std::this_thread::yield(); }
}

bool ThreadPool::is_idle() const { return m_queue.empty() && m_busy <= 0; }
} // namespace levk
