#pragma once
#include <levk/util/ptr.hpp>
#include <memory>

namespace levk {
class MeshGeometry {
  public:
	template <typename T>
	MeshGeometry(T t) : m_model(std::make_unique<Model<T>>(std::move(t))) {}

	template <typename T>
	Ptr<T> as() const {
		if (auto* p = dynamic_cast<Model<T>*>(m_model.get())) { return &p->impl; }
		return {};
	}

	std::uint32_t vertex_count() const { return m_model->vertex_count(); }
	std::uint32_t index_count() const { return m_model->index_count(); }
	bool has_joints() const { return m_model->has_joints(); }

  protected:
	struct Base {
		virtual ~Base() = default;

		virtual std::uint32_t vertex_count() const = 0;
		virtual std::uint32_t index_count() const = 0;
		virtual bool has_joints() const = 0;
	};

	template <typename T>
	struct Model : Base {
		T impl;
		Model(T&& t) : impl(std::move(t)) {}

		std::uint32_t vertex_count() const final { return gfx_mesh_vertex_count(impl); }
		std::uint32_t index_count() const final { return gfx_mesh_index_count(impl); }
		bool has_joints() const final { return gfx_mesh_has_joints(impl); }
	};

	std::unique_ptr<Base> m_model{};
};
} // namespace levk
