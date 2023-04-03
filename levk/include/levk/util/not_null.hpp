#pragma once
#include <levk/util/ptr.hpp>
#include <cassert>
#include <concepts>

namespace levk {
template <typename Type>
concept PointerLike = requires(Type const& t) {
						  !std::same_as<Type, std::nullptr_t>;
						  t != nullptr;
					  };

template <PointerLike Type>
class NotNull {
  public:
	NotNull(std::nullptr_t) = delete;
	NotNull& operator=(std::nullptr_t) = delete;

	template <std::convertible_to<Type> T>
	constexpr NotNull(T&& u) : m_t(std::forward<T>(u)) {
		assert(m_t != nullptr);
	}

	constexpr NotNull(Type t) : m_t(std::move(t)) { assert(m_t != nullptr); }

	template <std::convertible_to<Type> T>
	constexpr NotNull(NotNull<T> const& rhs) : m_t(rhs.get()) {}

	constexpr Type const& get() const { return m_t; }
	constexpr operator Type() const { return get(); }

	constexpr decltype(auto) operator*() const { return *get(); }
	constexpr decltype(auto) operator->() const { return get(); }

  private:
	Type m_t{};
};
} // namespace levk
