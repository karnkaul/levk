#pragma once
#include <levk/input.hpp>
#include <levk/transform.hpp>
#include <levk/util/ptr.hpp>
#include <levk/util/time.hpp>
#include <memory>

namespace levk {
template <typename Type>
concept TransformControllerT = requires(Type& type, Transform& t, Input const& i, Time dt) { type.tick(t, i, dt); };

class TransformController {
  public:
	template <TransformControllerT T>
	TransformController(T t) : m_model(std::make_unique<Model<T>>(std::move(t))) {}

	void tick(Transform& transform, Input const& input, Time dt) { m_model->tick(transform, input, dt); }

	template <typename T>
	Ptr<T> as() const {
		if (auto* p = dynamic_cast<Model<T>*>(m_model.get())) { return &p->impl; }
		return {};
	}

  private:
	struct Base {
		virtual ~Base() = default;

		virtual void tick(Transform& transform, Input const& input, Time dt) = 0;
	};

	template <typename T>
	struct Model : Base {
		T t;
		Model(T&& t) : t(std::move(t)) {}
		void tick(Transform& transform, Input const& input, Time dt) final { t.tick(transform, input, dt); }
	};

	std::unique_ptr<Base> m_model{};
};
} // namespace levk
