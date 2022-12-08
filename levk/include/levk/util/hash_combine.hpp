#pragma once
#include <functional>

namespace levk {
// boost::hash_combine
// https://www.boost.org/doc/libs/1_55_0/doc/html/hash/reference.html#boost.hash_combine
constexpr void hash_combine(std::size_t& out_seed, std::size_t hash) { out_seed ^= hash + 0x9e3779b9 + (out_seed << 6) + (out_seed >> 2); }

template <template <typename> typename Hasher = std::hash, typename... Types>
void hash_combine(std::size_t& out_seed, Types const&... t) {
	(hash_combine(out_seed, Hasher<Types>{}(t)), ...);
}

template <template <typename> typename Hasher = std::hash, typename... Types>
std::size_t make_combined_hash(Types const&... t) {
	auto ret = std::size_t{};
	hash_combine<Hasher>(ret, t...);
	return ret;
}
} // namespace levk
