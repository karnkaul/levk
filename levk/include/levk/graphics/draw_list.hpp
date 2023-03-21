#pragma once
#include <levk/graphics/common.hpp>
#include <levk/graphics/mesh.hpp>
#include <levk/util/not_null.hpp>
#include <span>
#include <variant>
#include <vector>

namespace levk {
class MaterialProvider;

struct Drawable {
	struct Instanced {
		NotNull<Primitive::Static*> primitive;
		NotNull<Material const*> material;
		glm::mat4 parent{1.0f};
		std::span<Transform const> instances{};
	};

	struct Dynamic {
		NotNull<Primitive::Dynamic*> primitive;
		NotNull<Material const*> material;
		glm::mat4 parent{1.0f};
		std::span<Transform const> instances{};
	};

	struct Skinned {
		NotNull<Primitive::Skinned*> primitive;
		NotNull<Material const*> material;
		std::span<glm::mat4 const> inverse_bind_matrices{};
		std::span<glm::mat4 const> joints{};
	};

	using Payload = std::variant<Instanced, Skinned, Dynamic>;

	Payload payload;

	Drawable(Instanced instanced) : payload(std::move(instanced)) {}
	Drawable(Skinned skinned) : payload(std::move(skinned)) {}
	Drawable(Dynamic dynamic) : payload(std::move(dynamic)) {}
};

class DrawList {
  public:
	virtual ~DrawList() = default;

	struct Instanced {
		glm::mat4 parent{1.0f};
		std::span<Transform const> instances{};
	};

	struct Skinned {
		std::span<glm::mat4 const> inverse_bind_matrices{};
		std::span<glm::mat4 const> joints{};
	};

	Extent2D extent() const { return m_extent; }
	Rect2D<> rect() const { return Rect2D<>::from_extent(extent()); }

	void add(NotNull<Primitive::Static*> primitive, NotNull<Material const*> material, Instanced const& instances);
	void add(NotNull<Primitive::Dynamic*> primitive, NotNull<Material const*> material, Instanced const& instances);
	void add(NotNull<Primitive::Skinned*> primitive, NotNull<Material const*> material, Skinned const& skin);
	void add(NotNull<Primitive::Static*> primitive, NotNull<Material const*> material, Transform const& transform);
	void add(NotNull<Primitive::Dynamic*> primitive, NotNull<Material const*> material, Transform const& transform);

	void add(NotNull<StaticMesh const*> mesh, Instanced const& instances, MaterialProvider& material_provider);
	void add(NotNull<StaticMesh const*> mesh, glm::mat4 const& transform, MaterialProvider& material_provider);

	void add(NotNull<SkinnedMesh const*> mesh, std::span<glm::mat4 const> joints, MaterialProvider& provider);

	std::span<Drawable const> drawables() const { return m_drawables; }

  protected:
	std::vector<Drawable> m_drawables{};
	Extent2D m_extent{};
};
} // namespace levk
