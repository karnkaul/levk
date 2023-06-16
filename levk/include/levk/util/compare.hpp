#pragma once

namespace levk::util {
template <typename Type>
concept ComparableT = requires(Type const& a, Type const& b) { a <=> b; };

enum class CompareOp { eEq, eNe, eLt, eLe, eGt, eGe };

struct Compare {
	using Op = CompareOp;

	Op type{Op::eEq};

	template <ComparableT Type>
	constexpr bool operator()(Type const& a, Type const& b) const {
		auto const result = a <=> b;
		switch (type) {
		default:
		case Op::eEq: return result == 0;
		case Op::eNe: return result != 0;
		case Op::eLt: return result < 0;
		case Op::eLe: return result <= 0;
		case Op::eGt: return result > 0;
		case Op::eGe: return result >= 0;
		}
	}
};
} // namespace levk::util
