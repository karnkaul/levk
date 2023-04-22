#pragma once
#include <levk/util/thread_pool.hpp>
#include <atomic>
#include <utility>

namespace levk {
template <typename ResultT>
class AsyncTask {
  public:
	virtual ~AsyncTask() = default;

	float progress() const { return m_progress.load(); }

	std::string_view status() const {
		auto lock = std::scoped_lock{m_mutex};
		return m_status;
	}

	bool valid() const { return get_future().future.valid() || !m_started; }
	bool started() const { return m_started; }
	bool running() const { return get_future().future.valid() && started(); }
	bool ready() const { return get_future().future.valid() && get_future().future.wait_for(std::chrono::seconds{}) == std::future_status::ready; }
	void wait() const { get_future().future.valid() && get_future().wait(); }

	ResultT get()
		requires(!std::is_void_v<ResultT>)
	{
		return get_future().get();
	}

	void run(ThreadPool& thread_pool) {
		m_started = true;
		get_future() = thread_pool.submit([this, &thread_pool] { return execute(thread_pool); });
	}

  protected:
	void set_status(std::string status) {
		auto lock = std::scoped_lock{m_mutex};
		m_status = std::move(status);
	}

	void set_progress(float progress) { m_progress.store(progress); }

	void wait() { get_future().wait(); }

	virtual ScopedFuture<ResultT> const& get_future() const = 0;
	virtual ResultT execute(ThreadPool& thread_pool) = 0;

	ScopedFuture<ResultT>& get_future() { return const_cast<ScopedFuture<ResultT>&>(std::as_const(*this).get_future()); }

  private:
	mutable std::mutex m_mutex{};
	std::string m_status{};
	std::atomic<float> m_progress{};
	bool m_started{};
};
} // namespace levk
