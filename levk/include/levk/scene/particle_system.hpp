#pragma once
#include <glm/gtx/quaternion.hpp>
#include <levk/graphics/geometry.hpp>
#include <levk/graphics/material.hpp>
#include <levk/graphics/primitive.hpp>
#include <levk/scene/component.hpp>
#include <levk/util/radians.hpp>
#include <variant>

namespace levk {
struct Particle {
	struct Config;
	struct Translater;
	struct Rotater;
	struct Scaler;
	struct Tinter;
	class Emitter;

	using Modifier = std::variant<Translater, Rotater, Scaler, Tinter>;

	template <typename Type>
	using Range = std::pair<Type, Type>;

	struct {
		glm::vec3 linear{};
		Radians angular{};
	} velocity{};

	struct {
		Range<Rgba> tint{};
		Range<glm::vec2> scale{};
	} lerp{};

	Duration ttl{};

	glm::vec3 position{};
	Radians rotation{};
	glm::vec2 scale{};
	Rgba tint{};
	Duration elapsed{};
	float alpha{};
};

struct Particle::Config {
	struct {
		Range<glm::vec3> position{};
		Range<Radians> rotation{};
	} initial{};

	struct {
		Range<glm::vec3> linear{glm::vec3{-1.0f}, glm::vec3{1.0f}};
		Range<Radians> angular{};
	} velocity{};

	struct {
		Range<Rgba> tint{white_v, white_v};
		Range<glm::vec2> scale{glm::vec3{1.0f}, glm::vec3{1.0f}};
	} lerp{};

	Range<Duration> ttl{10s, 10s};
	glm::vec2 quad_size{100.0f};
	std::size_t count{100};
};

struct Particle::Translater {
	void tick(Particle& out, Duration dt) const { out.position += out.velocity.linear * dt.count(); }
};

struct Particle::Rotater {
	void tick(Particle& out, Duration dt) const { out.rotation.value += out.velocity.angular.value * dt.count(); }
};

struct Particle::Scaler {
	void tick(Particle& out, Duration) const { out.scale = glm::mix(out.lerp.scale.first, out.lerp.scale.second, out.alpha); }
};

struct Particle::Tinter {
	void tick(Particle& out, Duration) const { out.tint.channels = glm::mix(out.lerp.tint.first.channels, out.lerp.tint.second.channels, out.alpha); }
};

class Particle::Emitter {
  public:
	Emitter();
	Emitter(vulkan::Device& vulkan_device);

	Config config{};
	UnlitMaterial material{};
	std::vector<Modifier> modifiers{};

	void tick(glm::quat const& view, Duration dt);
	void render(DrawList& out) const;

  private:
	Particle make_particle() const;
	void mirror(Particle const& particle, glm::quat const& view, std::size_t const start_index);
	void append_quad(glm::quat const& view, Particle const& particle);

	DynamicPrimitive m_primitive;
	std::vector<Particle> m_particles{};
};

class ParticleSystem : public RenderComponent {
  public:
	std::vector<Particle::Emitter> emitters{};

  private:
	void tick(Duration dt) final;
	void render(DrawList& out) const final;
};
} // namespace levk
