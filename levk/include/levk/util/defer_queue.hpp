#pragma once
#include <cstddef>
#include <memory>
#include <mutex>
#include <vector>

namespace levk {
///
/// \brief Concurrent queue of miscellaneous storage whose destruction is deferred by a few frames of updates.
///
/// Extremely low level data structure, used solely by Vulkan wrappers to defer destruction of GPU resources.
///
class DeferQueue {
  public:
	DeferQueue() = default;
	DeferQueue& operator=(DeferQueue&&) = delete;

	~DeferQueue() { clear(); }

	///
	/// \brief Push an item into the queue.
	/// \param t Item to move into the queue
	///
	template <typename T>
	void push(T t) {
		auto lock = std::scoped_lock{m_mutex};
		m_stack.push(std::move(t));
	}

	///
	/// \brief Advance the queue, destroying items whose defer count is 0.
	///
	void next() {
		auto lock = std::scoped_lock{m_mutex};
		m_stack.next();
	}

	///
	/// \brief Clear the queue.
	///
	void clear() {
		auto lock = std::scoped_lock{m_mutex};
		m_stack = {};
	}

  private:
	struct Erased {
		virtual ~Erased() = default;
	};
	template <typename T>
	struct Model : Erased {
		T t;
		Model(T&& t) : t{std::move(t)} {};
	};

	template <std::size_t Size = 3>
	struct Stack {
		using Row = std::vector<std::unique_ptr<Erased>>;

		Row rows[Size]{};
		std::size_t index{};

		template <typename T>
		void push(T t) {
			rows[index].push_back(std::make_unique<Model<T>>(std::move(t)));
		}

		void next() {
			index = (index + 1) % Size;
			rows[index].clear();
		}
	};

	Stack<> m_stack{};
	std::mutex m_mutex{};
};
} // namespace levk
