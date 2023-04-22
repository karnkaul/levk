#pragma once
#include <levk/imcpp/common.hpp>
#include <levk/imcpp/input_text.hpp>
#include <levk/scene/entity.hpp>

namespace levk::imcpp {
class Inspector {
  public:
	enum class Type { eEntity, eSceneCamera, eLights };

	struct Target {
		Id<Entity> entity{};
		Type type{};

		explicit constexpr operator bool() const { return type != Type::eEntity || entity != Id<Entity>{}; }

		constexpr bool operator==(Id<Entity> id) const { return type == Type::eEntity && id == entity; }
		constexpr bool operator==(Type desired) const { return type == desired; }
	};

	Target target{};
	float width_pct{0.35f};

	void display(Scene& scene);

  private:
	struct EntityName {
		Id<Entity> previous{};
		imcpp::InputText<> input_text{};
	};

	void draw_to(NotClosed<Window> w, Scene& scene);
	imcpp::InputText<>& get_entity_name(Id<Entity> id, std::string_view name);

	EntityName m_entity_name{};
};
} // namespace levk::imcpp
