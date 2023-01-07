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
}

namespace levk {
template <typename T>
concept ComponentT = std::derived_from<T, Component>;

class Entity {
  public:
	struct Renderer : ISerializable {
		virtual ~Renderer() = default;

		virtual void render(Entity const& entity) const = 0;
	};

	template <ComponentT T>
	T& attach(std::unique_ptr<T>&& t) {
		assert(t);
		auto& ret = *t;
		attach(TypeId::make<T>().value(), std::move(t));
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

	template <ComponentT T>
	void detach() {
		m_components.erase(TypeId::make<T>());
	}

	Id<Entity> id() const { return m_id; }
	Id<Node> node_id() const { return m_node; }
	Ptr<Renderer> renderer() const { return m_renderer.get(); }

	void tick(Time dt);

	Scene& scene() const;

  private:
	void attach(TypeId::value_type type_id, std::unique_ptr<Component>&& component);

	std::unordered_map<TypeId::value_type, std::unique_ptr<Component>> m_components{};
	std::vector<Ptr<Component>> m_sorted{};
	Id<Entity> m_id{};
	Id<Node> m_node{};
	Ptr<Scene> m_scene{};
	std::unique_ptr<Renderer> m_renderer{};

	friend class Scene;
};
} // namespace levk
