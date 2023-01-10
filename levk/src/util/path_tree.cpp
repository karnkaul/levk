#include <fmt/format.h>
#include <levk/util/path_tree.hpp>
#include <algorithm>

namespace levk {
void PathTree::Builder::add(std::string_view path) {
	auto i = path.find_first_of('/');
	auto* target = &root;
	while (i != std::string::npos) {
		auto dir = std::string{path.substr(0, i + 1)};
		if (auto i = target->children.find(dir); i != target->children.end()) {
			target = i->second.get();
		} else {
			auto entry = std::make_unique<Entry>(Entry{std::string{dir}});
			auto [it, _] = target->children.insert_or_assign(std::string{dir}, std::move(entry));
			target = it->second.get();
		}
		path = path.substr(i + 1);
		i = path.find_first_of('/');
	}
	target->children.insert_or_assign(std::string{path}, std::make_unique<Entry>(Entry{std::string{path}}));
}

void sort_inplace(PathTree& out) {
	for (auto& child : out.children) { sort_inplace(child); }
	std::sort(out.children.begin(), out.children.end(), [](PathTree const& a, PathTree const& b) { return a.path < b.path; });
}

void add_to(std::vector<PathTree>& out, std::unordered_map<std::string, std::unique_ptr<PathTree::Builder::Entry>>& map, std::string parent) {
	out.reserve(map.size());
	for (auto& [name, in_child] : map) {
		auto out_child = PathTree{};
		out_child.name = std::move(name);
		out_child.path = fmt::format("{}{}", parent, out_child.name);
		add_to(out_child.children, in_child->children, out_child.path);
		out.push_back(std::move(out_child));
	}
}

PathTree PathTree::Builder::build() {
	auto ret = PathTree{.name = std::move(root.name)};
	add_to(ret.children, root.children, root.name);
	sort_inplace(ret);
	return ret;
}
} // namespace levk
