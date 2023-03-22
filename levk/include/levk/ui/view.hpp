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

class View {
  public:
	static constexpr auto frame_v{Rect::from_extent({100.0f, 100.0f})};

	virtual ~View() = default;

	View& operator=(View&&) = delete;

	View(Rect frame = frame_v) : m_frame(frame) {}

	Ptr<View> super_view() const { return m_super_view; }
	Rect const& frame() const { return m_frame; }
	Rect world_frame() const;
	void set_frame(Rect frame);
	void set_position(glm::vec2 position);
	void set_extent(glm::vec2 extent);

	Ptr<View> add_sub_view(std::unique_ptr<View> view);
	void set_destroyed() { m_destroyed = true; }
	bool is_destroyed() const { return m_destroyed; }

	std::span<std::unique_ptr<View> const> sub_views() const { return m_sub_views; }

	virtual void tick(Input const& input, Time dt);
	virtual void render(DrawList& out) const;

	glm::vec2 n_anchor{};
	float z_index{};
	float z_rotation{};

  private:
	Rect m_frame{};
	std::vector<std::unique_ptr<View>> m_sub_views{};
	Ptr<View> m_super_view{};
	bool m_destroyed{};
};
} // namespace ui
} // namespace levk
