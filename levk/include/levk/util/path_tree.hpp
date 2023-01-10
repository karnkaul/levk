#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace levk {
struct PathTree {
	struct Builder;

	std::string name{};
	std::string path{};
	std::vector<PathTree> children{};

	bool is_directory() const { return !name.empty() && name.back() == '/'; }
};

struct PathTree::Builder {
	struct Entry {
		std::string name{};
		std::unordered_map<std::string, std::unique_ptr<Entry>> children{};
	};

	Entry root{"/"};

	void add(std::string_view path);
	PathTree build();
};
} // namespace levk
