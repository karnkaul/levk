#pragma once
#include <levk/util/bool.hpp>
#include <levk/util/ptr.hpp>
#include <levk/util/thread_pool.hpp>
#include <levk/util/visitor.hpp>
#include <variant>

namespace levk {
template <typename Type>
class Futopt {
  public:
	Futopt() = default;

	template <typename F>
		requires(std::same_as<std::invoke_result_t<F>, Type>)
	explicit Futopt(F func) : m_value(std::optional<Type>{func()}) {}

	template <typename F>
		requires(std::same_as<std::invoke_result_t<F>, Type>)
	Futopt(ThreadPool& pool, F func) : m_value(ScopedFuture<Type>{pool, std::move(func)}) {}

	bool active() const {
		auto const visitor = Visitor{
			[](ScopedFuture<Type> const& future) { return future.future.valid(); },
			[](std::optional<Type> const& optional) { return optional.has_value(); },
		};
		return std::visit(visitor, m_value);
	}

	Type const& get() const {
		assert(active());
		auto const visitor = Visitor{
			[](ScopedFuture<Type> const& future) -> Type const& { return future.future.get(); },
			[](std::optional<Type> const& optional) -> Type const& { return *optional; },
		};
		return std::visit(visitor, m_value);
	}

	Type& get() { return const_cast<Type&>(std::as_const(*this).get()); }

	void wait() const {
		if (!std::holds_alternative<ScopedFuture<Type>>(m_value)) { return; }
		std::get<ScopedFuture<Type>>(m_value).wait();
	}

  private:
	std::variant<ScopedFuture<Type>, std::optional<Type>> m_value;
};

template <>
class Futopt<void> {
  public:
	Futopt() = default;

	template <typename F>
		requires(std::same_as<std::invoke_result_t<F>, void>)
	Futopt(ThreadPool& pool, F func) : m_value(ScopedFuture<void>{pool, std::move(func)}) {}

	template <typename F>
		requires(std::same_as<std::invoke_result_t<F>, void>)
	explicit Futopt(F func) : m_value(Bool{true}) {
		func();
	}

	bool active() const {
		auto const visitor = Visitor{
			[](ScopedFuture<void> const& future) { return future.future.valid(); },
			[](Bool const is_set) { return is_set.value; },
		};
		return std::visit(visitor, m_value);
	}

	void wait() const {
		if (!std::holds_alternative<ScopedFuture<void>>(m_value)) { return; }
		std::get<ScopedFuture<void>>(m_value).wait();
	}

  private:
	std::variant<ScopedFuture<void>, Bool> m_value;
};

template <typename F>
auto make_futopt(Ptr<ThreadPool> pool, F func) -> Futopt<std::invoke_result_t<F>> {
	if (pool) { return {*pool, std::move(func)}; }
	return Futopt<std::invoke_result_t<F>>{std::move(func)};
}
} // namespace levk
