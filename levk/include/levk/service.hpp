#pragma once
#include <levk/util/ptr.hpp>
#include <atomic>
#include <cassert>
#include <utility>

namespace levk {
template <typename Type>
class Service {
  public:
	using value_type = Type;

	class Instance;

	static void provide(Ptr<Type> t) { s_t.store(t); }
	static void unprovide() { s_t.store(nullptr); }
	static Ptr<Type> find() { return s_t.load(); }
	static bool contains() { return find() != nullptr; }
	static Type& get() { return (assert(contains()), *find()); }
	static Type& locate() { return get(); }

  private:
	inline static std::atomic<Ptr<Type>> s_t{};
};

template <typename Type>
class Service<Type>::Instance {
  public:
	Instance& operator=(Instance&&) = delete;

	template <typename... Args>
		requires(std::is_constructible_v<Type, Args...>)
	Instance(Args&&... args) : m_t(std::forward<Args>(args)...) {
		provide(&m_t);
	}

	~Instance() { unprovide(); }

	Type& get() { return m_t; }
	Type const& get() const { return m_t; }

  private:
	Type m_t{};
};
} // namespace levk
