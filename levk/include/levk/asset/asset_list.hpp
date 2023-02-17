#pragma once
#include <levk/uri.hpp>
#include <unordered_set>

namespace levk {
struct AssetList {
	using Set = std::unordered_set<Uri<>, Uri<>::Hasher>;

	Set meshes{};
	Set materials{};
	Set skeletons{};
	Set animations{};
};
} // namespace levk
