#pragma once
#include <levk/component.hpp>
#include <levk/util/id.hpp>
#include <levk/util/ptr.hpp>
#include <levk/util/type_id.hpp>
#include <cassert>
#include <memory>
#include <unordered_map>
#include <vector>

namespace levk {
class Node;

template <typename T>
concept ComponentT = std::derived_from<T, Component>;

class Entity final {
  public:
	///
	/// \brief Each Entity can have zero or one (concrete) Renderer instances attached.
	///
	struct Renderer : Component {
		virtual ~Renderer() = default;

		void tick(Time) final {}
		virtual void render() const = 0;
	};

	template <ComponentT T>
	T& attach(std::unique_ptr<T>&& t) {
		assert(t);
		auto& ret = *t;
		if constexpr (std::derived_from<T, Renderer>) {
			set_renderer(std::move(t));
		} else {
			attach(TypeId::make<T>().value(), std::move(t));
		}
		return ret;
	}

	template <ComponentT T>
	Ptr<T> find() const {
		if (auto it = m_components.find(TypeId::make<T>().value()); it != m_components.end()) { return &static_cast<T&>(*it->second); }
		return {};
	}

	template <ComponentT T>
	T& get() const {
		auto ret = find<T>();
		assert(ret);
		return *ret;
	}

	template <std::derived_from<Renderer> T>
	Ptr<T> renderer_as() const {
		return dynamic_cast<T*>(m_renderer.get());
	}

	template <ComponentT T>
	void detach() {
		if constexpr (std::derived_from<T, Renderer>) {
			if (dynamic_cast<T*>(m_renderer.get())) { m_renderer.reset(); }
		} else {
			m_components.erase(TypeId::make<T>());
		}
	}

	Id<Entity> id() const { return m_id; }
	Id<Node> node_id() const { return m_node; }

	void tick(Time dt);

	Scene& scene() const;

	template <typename ContainerT>
	void fill_components_to(ContainerT&& container) const {
		for (auto const& [_, component] : m_components) { container.insert(container.end(), *component); }
	}

	bool active{true};

  private:
	void init(Component& out) const;
	void attach(TypeId::value_type type_id, std::unique_ptr<Component>&& component);
	void set_renderer(std::unique_ptr<Renderer>&& renderer);

	std::unordered_map<TypeId::value_type, std::unique_ptr<Component>> m_components{};
	std::vector<Ptr<Component>> m_sorted{};
	Id<Entity> m_id{};
	Id<Node> m_node{};
	Ptr<Scene> m_scene{};
	std::unique_ptr<Renderer> m_renderer{};

	friend class Scene;
};
} // namespace levk
