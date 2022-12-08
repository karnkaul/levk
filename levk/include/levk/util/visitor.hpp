#pragma once

namespace levk {
///
/// \brief Wrapper for constructing overloaded visitors for std::visit.
///
template <typename... T>
struct Visitor : T... {
	using T::operator()...;
};

///
/// \brief Deduction guide for Visitor<T...>.
///
template <typename... T>
Visitor(T...) -> Visitor<T...>;
} // namespace levk
