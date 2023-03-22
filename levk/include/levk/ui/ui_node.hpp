#pragma once
#include <levk/rect.hpp>
#include <levk/util/ptr.hpp>
#include <levk/util/time.hpp>
#include <memory>
#include <vector>

namespace levk {
struct Input;
class DrawList;

using UIRect = Rect2D<>;

class UINode {
  public:
	virtual ~UINode() = default;

	UINode& operator=(UINode&&) = delete;

	Ptr<UINode> super_node() const { return m_super_node; }
	glm::vec2 world_position() const;

	Ptr<UINode> add_sub_node(std::unique_ptr<UINode> node);
	void set_destroyed() { m_destroyed = true; }
	bool is_destroyed() const { return m_destroyed; }

	std::span<std::unique_ptr<UINode> const> sub_nodes() const { return m_sub_nodes; }

	virtual void tick(Input const& input, Time dt);
	virtual void render(DrawList& out) const;

	glm::vec2 position{};
	float z_index{};
	float z_rotation{};

  private:
	std::vector<std::unique_ptr<UINode>> m_sub_nodes{};
	Ptr<UINode> m_super_node{};
	bool m_destroyed{};
};
} // namespace levk
