#pragma once
#include <atomic>
#include <future>

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

	bool valid() const { return m_future.valid() || !m_started; }
	bool started() const { return m_started; }
	bool running() const { return m_future.valid() && started(); }
	bool ready() const { return m_future.valid() && m_future.wait_for(std::chrono::seconds{}) == std::future_status::ready; }
	void wait() const { m_future.valid() && m_future.wait(); }

	ResultT get()
		requires(!std::is_void_v<ResultT>)
	{
		return m_future.get();
	}

	void run() {
		m_started = true;
		m_future = std::async([this] { return execute(); });
	}

  protected:
	void set_status(std::string status) {
		auto lock = std::scoped_lock{m_mutex};
		m_status = std::move(status);
	}

	void set_progress(float progress) { m_progress.store(progress); }

	virtual ResultT execute() = 0;

  private:
	mutable std::mutex m_mutex{};
	std::future<ResultT> m_future{};
	std::string m_status{};
	std::atomic<float> m_progress{};
	bool m_started{};
};
} // namespace levk
