#pragma once
#include <levk/util/async_queue.hpp>
#include <levk/util/unique_task.hpp>
#include <atomic>
#include <future>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace levk {
///
/// \brief Pool of fixed number of threads and a single shared task queue.
///
class ThreadPool {
  public:
	///
	/// \brief Construct a ThreadPool.
	/// \param threads Number of threads to create
	///
	ThreadPool(std::optional<std::uint32_t> thread_count = {});
	~ThreadPool();

	///
	/// \brief Schedule a task to be run on the thread pool.
	/// \param func The task to enqueue.
	/// \returns Future of task invocation result
	///
	template <typename F>
	auto submit(F func) -> std::future<std::invoke_result_t<F>> {
		using Ret = std::invoke_result_t<F>;
		auto promise = std::promise<Ret>{};
		auto ret = promise.get_future();
		submit(std::move(promise), std::move(func));
		return ret;
	}

	///
	/// \brief Block until queue is empty and workers are idle.
	///
	void wait_idle();

	///
	/// \brief Check if any submitted tasks are pending execution.
	/// \returns true if any submitted tasks are pending execution
	///
	bool is_idle() const;

	///
	/// \brief Obtain the number of worker threads active on the task queue.
	/// \returns The number of worker threads active on the task queue
	///
	std::size_t thread_count() const { return m_threads.size(); }

  private:
	template <typename T, typename F>
	void submit(std::promise<T>&& promise, F func) {
		m_queue.push([p = std::move(promise), f = std::move(func)]() mutable {
			try {
				if constexpr (std::is_void_v<T>) {
					f();
					p.set_value();
				} else {
					p.set_value(f());
				}
			} catch (...) {
				try {
					p.set_exception(std::current_exception());
				} catch (...) {}
			}
		});
	}

	AsyncQueue<UniqueTask<void()>> m_queue{};
	std::vector<std::jthread> m_threads{};
	std::atomic<std::int32_t> m_busy{};
};

template <typename Type>
struct ScopedFuture {
	ScopedFuture() = default;
	ScopedFuture(ScopedFuture&&) = default;

	ScopedFuture(std::future<Type> future) : future(std::move(future).share()) {}

	~ScopedFuture() { wait(); }

	ScopedFuture& operator=(ScopedFuture&& rhs) noexcept {
		if (&rhs != this) {
			wait();
			future = std::move(rhs.future);
		}
		return *this;
	}

	void wait() const {
		if (future.valid()) { future.wait(); }
	}

	std::shared_future<Type> future{};
};
} // namespace levk
