#pragma once
#include <cassert>
#include <memory>

namespace levk {
template <typename T>
class UniqueTask;

///
/// \brief Type erased move-only callable (discount std::move_only_function).
///
template <typename Ret, typename... Args>
class UniqueTask<Ret(Args...)> {
  public:
	UniqueTask() = default;

	///
	/// \brief Construct an instance by passing a callable.
	/// \param t Callable of signature Ret(Args...)
	///
	template <typename T>
		requires(!std::same_as<UniqueTask, T> && std::is_invocable_r_v<Ret, T, Args...>)
	UniqueTask(T t) : m_func(std::make_unique<Func<T>>(std::move(t))) {}

	///
	/// \brief Invoke stored callable and obtain the result (if non void).
	/// \param args... Arguments to invoke with
	/// \returns Return value of callable (if any)
	///
	Ret operator()(Args... args) const {
		assert(m_func);
		return (*m_func)(args...);
	}

	///
	/// \brief Check if a callable is stored.
	/// \returns true If a callable is stored.
	///
	explicit operator bool() const { return m_func != nullptr; }

  private:
	struct Base {
		virtual ~Base() = default;
		virtual void operator()(Args...) = 0;
	};
	template <typename F>
	struct Func : Base {
		F f;
		Func(F&& f) : f(std::move(f)) {}
		void operator()(Args... args) final { f(args...); }
	};

	std::unique_ptr<Base> m_func{};
};
} // namespace levk
