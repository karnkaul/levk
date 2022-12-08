#pragma once
#include <levk/util/async_queue.hpp>
#include <levk/util/unique_task.hpp>
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
	auto enqueue(F func) -> std::future<std::invoke_result_t<F>> {
		using Ret = std::invoke_result_t<F>;
		auto promise = std::promise<Ret>{};
		auto ret = promise.get_future();
		push(std::move(promise), std::move(func));
		return ret;
	}

	///
	/// \brief Obtain the number of worker threads active on the task queue.
	/// \returns The number of worker threads active on the task queue
	///
	std::size_t thread_count() const { return m_threads.size(); }

  private:
	template <typename T, typename F>
	void push(std::promise<T>&& promise, F func) {
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
};

template <typename T>
struct StoredFuture {
	std::shared_future<T> future{};
	std::optional<T> t{};

	StoredFuture() = default;

	template <typename F>
		requires(std::same_as<std::invoke_result_t<F>, T>)
	StoredFuture(ThreadPool& pool, F func) : future(pool.enqueue([f = std::move(func)] { return f(); }).share()) {}

	template <typename F>
		requires(std::same_as<std::invoke_result_t<F>, T>)
	explicit StoredFuture(F func) : t(func()) {}

	bool active() const { return future.valid() || t.has_value(); }

	T const& get() const {
		assert(active());
		if (future.valid()) { return future.get(); }
		return *t;
	}

	T& get() { return const_cast<T&>(std::as_const(*this).get()); }
};
} // namespace levk
