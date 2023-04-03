#pragma once
#include <levk/graphics/transform_animation.hpp>
#include <levk/node.hpp>
#include <levk/uri.hpp>

namespace levk {
struct Skeleton {
	template <typename T>
	using Index = std::size_t;

	struct Animation;

	struct Instance {
		Id<Node> root{};
		std::vector<Id<Node>> joints{};
		std::vector<Animation> animations{};
		Uri<Skeleton> source{};
	};

	struct Joint {
		Transform transform{};
		Index<Joint> self{};
		std::vector<Index<Joint>> children{};
		std::optional<Index<Joint>> parent{};
		std::string name{};
	};

	struct Animation {
		struct Source {
			TransformAnimation animation{};
			std::vector<Index<Joint>> target_joints{};
			std::string name{};
		};

		Uri<Skeleton> skeleton{};
		Index<Source> source{};
		std::vector<Id<Node>> target_nodes{};

		void update_nodes(Node::Locator node_locator, Time time, Source const& source) const;
	};

	std::vector<Joint> joints{};
	std::vector<Animation::Source> animation_sources{};
	std::string name{};
	Uri<Skeleton> self{};

	Instance instantiate(Node::Tree& out, Id<Node> root) const;

	void set_uri(Uri<Skeleton> uri) { self = std::move(uri); }
};
} // namespace levk
