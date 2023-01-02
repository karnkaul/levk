#pragma once
#include <levk/interpolator.hpp>
#include <levk/node.hpp>
#include <levk/util/id.hpp>
#include <memory>
#include <variant>

namespace levk {
template <typename Type>
concept AnimatorT = requires(Type const& t, Node& node, Time time) {
						std::is_copy_constructible_v<Type>;
						{ t.target } -> std::convertible_to<Id<Node>>;
						{ t.duration() } -> std::convertible_to<Time>;
						{ t.update(node, time) };
					};

struct TransformAnimator {
	struct Translate : Interpolator<glm::vec3> {};
	struct Rotate : Interpolator<glm::quat> {};
	struct Scale : Interpolator<glm::vec3> {};
	using Sampler = std::variant<Translate, Rotate, Scale>;

	Sampler sampler{};
	Id<Node> target{};

	Time duration() const;
	void update(Node& node, Time time) const;
};

static_assert(AnimatorT<TransformAnimator>);

class Animator {
  public:
	Animator(Animator&&) = default;
	Animator& operator=(Animator&&) = default;
	Animator(Animator const& rhs) : m_model(clone(rhs.m_model)) {}
	Animator& operator=(Animator const& rhs) { return (m_model = clone(rhs.m_model), *this); }

	template <AnimatorT T>
	Animator(T t) : m_model(std::make_unique<Model<T>>(std::move(t))) {}

	Id<Node> target() const { return m_model->target(); }
	Time duration() const { return m_model->duration(); }
	void update(Node::Locator node_locator, Time time) const { m_model->update(node_locator, time); }

	template <typename T>
	Ptr<T> as() const {
		if (auto* p = dynamic_cast<Model<T>*>(m_model.get())) { return &p->impl; }
		return {};
	}

  private:
	struct Base {
		virtual ~Base() = default;

		virtual Id<Node> target() const = 0;
		virtual Time duration() const = 0;
		virtual void update(Node::Locator, Time) const = 0;
		virtual std::unique_ptr<Base> clone() const = 0;
	};

	template <AnimatorT T>
	struct Model : Base {
		T impl;
		Model(T&& t) : impl(std::move(t)) {}

		Id<Node> target() const final { return impl.target; }
		Time duration() const final { return impl.duration(); }
		void update(Node::Locator locator, Time time) const final {
			if (auto* node = locator.find(target())) { impl.update(*node, time); }
		}
		std::unique_ptr<Base> clone() const final { return std::make_unique<Model<T>>(*this); }
	};

	static std::unique_ptr<Base> clone(std::unique_ptr<Base> const& other) {
		if (!other) { return {}; }
		return other->clone();
	}

	std::unique_ptr<Base> m_model{};
};
} // namespace levk
