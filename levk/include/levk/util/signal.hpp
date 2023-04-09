#pragma once
#include <levk/util/ptr.hpp>
#include <algorithm>
#include <functional>
#include <memory>
#include <vector>

namespace levk {
template <typename... Args>
class Signal {
  public:
	using Callback = std::function<void(Args const&...)>;

	class Listener;

	class Handle;

	Signal() = default;
	Signal(Signal&&) = default;
	Signal& operator=(Signal&&) = default;

	Signal(Signal const&) = delete;
	Signal& operator=(Signal const&) = delete;

	Handle connect(Callback callback) {
		if (!m_storage) { return {}; }
		auto id = ++m_prev_id;
		m_storage->push_back({std::move(callback), id});
		return Handle{m_storage, id};
	}

	template <typename Type, typename Func>
	Handle connect(Ptr<Type> object, Func member_func) {
		return connect([object, member_func](Args const&... args) { (object->*member_func)(args...); });
	}

	void dispatch(Args const&... args) const {
		if (!m_storage) { return; }
		auto storage = *m_storage;
		for (auto const& entry : storage) { entry.callback(args...); }
	}

	void operator()(Args const&... args) const { dispatch(args...); }

	bool empty() const { return m_storage->empty(); }

  private:
	struct Entry {
		Callback callback{};
		std::uint64_t id{};
	};

	using Storage = std::vector<Entry>;

	std::shared_ptr<Storage> m_storage{std::make_shared<Storage>()};
	std::uint64_t m_prev_id{};
};

template <typename... Args>
class Signal<Args...>::Handle {
  public:
	Handle() = default;

	bool active() const {
		auto storage = m_storage.lock();
		if (!storage) { return false; }
		auto const func = [id = m_id](Entry const& e) { return e.id == id; };
		return std::find_if(storage->begin(), storage->end(), func) != storage->end();
	}

	void disconnect() {
		if (!m_id) { return; }
		auto storage = m_storage.lock();
		if (!storage) { return; }
		std::erase_if(*storage, [id = m_id](Entry const& e) { return e.id == id; });
		m_id = {};
	}

	explicit operator bool() const { return active(); }

  private:
	Handle(std::shared_ptr<Storage> const& storage, std::uint64_t id) : m_storage(storage), m_id(id) {}

	std::weak_ptr<Storage> m_storage{};
	std::uint64_t m_id{};

	friend class Signal;
};

template <typename... Args>
class Signal<Args...>::Listener {
  public:
	Listener() = default;

	Listener(Listener&&) = default;
	Listener& operator=(Listener&& rhs) noexcept {
		if (&rhs != this) {
			disconnect();
			m_handle = std::move(rhs.m_handle);
		}
		return *this;
	}

	Listener& operator=(Listener const&) = delete;
	Listener(Listener const&) = delete;

	Listener(Handle handle) : m_handle(std::move(handle)) {}
	~Listener() { disconnect(); }

	Handle const& handle() const { return m_handle; }
	void disconnect() { m_handle.disconnect(); }

	bool connected() const { return static_cast<bool>(m_handle); }

	explicit operator bool() const { return connected(); }

  private:
	Handle m_handle{};
};
} // namespace levk
