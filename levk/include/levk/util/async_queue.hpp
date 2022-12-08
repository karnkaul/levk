#pragma once
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <stop_token>

namespace levk {
///
/// \brief Basic multi-producer, multi-consumer FIFO queue
///
template <typename Type>
class AsyncQueue {
  public:
	///
	/// \brief Push t to the back
	///
	void push(Type t) {
		auto lock = std::unique_lock{m_mutex};
		m_queue.push_back(std::move(t));
		lock.unlock();
		m_cv.notify_one();
	}

	///
	/// \brief Push [*first, *last) in order to the back
	///
	template <typename It>
	void push(It first, It last) {
		auto lock = std::unique_lock{m_mutex};
		m_queue.insert(m_queue.end(), first, last);
		lock.unlock();
		m_cv.notify_all();
	}

	///
	/// \brief Try to dequeue an item: non-blocking
	/// \returns std::nullopt if queue is empty, else the front
	///
	std::optional<Type> try_pop() {
		auto lock = std::unique_lock{m_mutex};
		if (m_queue.empty()) { return {}; }
		return pop_front(lock);
	}

	///
	/// \brief Try to dequeue an item: blocking
	/// \returns std::nullopt if queue is empty or stop has been requested, else the front
	///
	std::optional<Type> pop(std::stop_token const& stop) {
		auto lock = std::unique_lock{m_mutex};
		m_cv.wait(lock, [this, stop] { return !m_queue.empty() || stop.stop_requested(); });
		if (stop.stop_requested() || m_queue.empty()) { return {}; }
		return pop_front(lock);
	}

	///
	/// \brief Notify all sleeping threads
	///
	void ping() { m_cv.notify_all(); }

	///
	/// \brief Empty queue into drain, notify all sleeping threads, and return drain
	/// \returns Items left in the queue
	///
	std::deque<Type> release() {
		auto lock = std::unique_lock{m_mutex};
		auto ret = std::move(m_queue);
		lock.unlock();
		m_cv.notify_all();
		return ret;
	}

  private:
	std::optional<Type> pop_front(std::unique_lock<std::mutex> const&) {
		auto ret = std::move(m_queue.front());
		m_queue.pop_front();
		return ret;
	}

	std::deque<Type> m_queue{};
	std::condition_variable m_cv{};
	std::mutex m_mutex{};
};
} // namespace levk
