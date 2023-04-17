#pragma once
#include <glm/mat4x4.hpp>
#include <levk/graphics/common.hpp>
#include <levk/graphics/drawable.hpp>
#include <levk/graphics/mesh.hpp>
#include <algorithm>
#include <variant>
#include <vector>

namespace levk {
class MaterialProvider;

class DrawList {
  public:
	virtual ~DrawList() = default;

	struct Instances {
		glm::mat4 parent{1.0f};
		std::span<Transform const> instances{};
	};

	struct Skin {
		std::span<glm::mat4 const> inverse_bind_matrices{};
		std::span<glm::mat4 const> joints_global_transforms{};
	};

	Extent2D extent() const { return m_extent; }
	Rect2D<> rect() const { return Rect2D<>::from_extent(extent()); }

	void add(NotNull<StaticPrimitive const*> primitive, NotNull<Material const*> material, Instances const& instances);
	void add(NotNull<DynamicPrimitive const*> primitive, NotNull<Material const*> material, Instances const& instances);
	// void add(NotNull<SkinnedPrimitive const*> primitive, NotNull<Material const*> material, Skin const& skin);
	void add(NotNull<StaticPrimitive const*> primitive, NotNull<Material const*> material, Transform const& transform);
	void add(NotNull<DynamicPrimitive const*> primitive, NotNull<Material const*> material, Transform const& transform);

	void add(NotNull<StaticMesh const*> mesh, Instances const& instances, MaterialProvider& material_provider);
	void add(NotNull<StaticMesh const*> mesh, glm::mat4 const& transform, MaterialProvider& material_provider);

	void add(NotNull<SkinnedMesh const*> mesh, std::span<glm::mat4 const> joints, glm::mat4 const& transform, MaterialProvider& provider);
	void add(NotNull<SkinnedMesh const*> mesh, std::span<glm::mat4 const> joints, Instances const& instances, MaterialProvider& provider);

	void import_skins(std::span<Skin const> skins) { m_skins = {skins.begin(), skins.end()}; }
	void add(Drawable drawable);

	std::span<Drawable const> drawables() const { return m_drawables; }
	std::span<Skin const> skins() const { return m_skins; }

	template <typename Func>
	void sort_by(Func func) {
		if (m_drawables.size() < 2) { return; }
		std::sort(m_drawables.begin(), m_drawables.end(), func);
	}

	void merge(DrawList const& rhs) { std::copy(rhs.m_drawables.begin(), rhs.m_drawables.end(), std::back_inserter(m_drawables)); }

  protected:
	std::vector<Drawable> m_drawables{};
	std::vector<Skin> m_skins{};
	Extent2D m_extent{};
};
} // namespace levk
