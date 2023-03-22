#pragma once
#include <levk/rect.hpp>
#include <levk/util/ptr.hpp>
#include <levk/util/time.hpp>
#include <memory>
#include <vector>

namespace levk {
struct Input;
class DrawList;

namespace ui {
using Rect = Rect2D<>;

class Node {
  public:
	static constexpr auto frame_v{Rect::from_extent({100.0f, 100.0f})};

	virtual ~Node() = default;

	Node& operator=(Node&&) = delete;

	Node(Rect frame = frame_v) : m_frame(frame) {}

	Ptr<Node> super_node() const { return m_super_node; }
	Rect const& frame() const { return m_frame; }
	Rect world_frame() const;
	void set_frame(Rect frame);
	void set_position(glm::vec2 position);
	void set_extent(glm::vec2 extent);

	Ptr<Node> add_sub_node(std::unique_ptr<Node> node);
	void set_destroyed() { m_destroyed = true; }
	bool is_destroyed() const { return m_destroyed; }

	std::span<std::unique_ptr<Node> const> sub_nodes() const { return m_sub_nodes; }

	virtual void tick(Input const& input, Time dt);
	virtual void render(DrawList& out) const;

	glm::vec2 n_anchor{};
	float z_index{};
	float z_rotation{};

  private:
	Rect m_frame{};
	std::vector<std::unique_ptr<Node>> m_sub_nodes{};
	Ptr<Node> m_super_node{};
	bool m_destroyed{};
};
} // namespace ui
} // namespace levk
