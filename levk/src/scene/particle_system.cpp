#include <levk/engine.hpp>
#include <levk/graphics/draw_list.hpp>
#include <levk/graphics/render_device.hpp>
#include <levk/scene/particle_system.hpp>
#include <levk/scene/scene.hpp>
#include <levk/service.hpp>
#include <levk/util/enumerate.hpp>
#include <levk/util/random.hpp>
#include <algorithm>

namespace levk {
namespace {
glm::vec3 random_vec3(Particle::Range<glm::vec3> const& range) {
	auto ret = glm::vec3{};
	ret.x = util::random_range(range.first.x, range.second.x);
	ret.y = util::random_range(range.first.y, range.second.y);
	ret.z = util::random_range(range.first.z, range.second.z);
	return ret;
}

template <typename... T>
void clear(T&... out) {
	(out.clear(), ...);
}

template <typename... T>
void reserve(std::size_t capacity, T&... out) {
	(out.reserve(capacity), ...);
}

template <typename... T>
void resize(std::size_t size, T&... out) {
	(out.resize(size), ...);
}

constexpr auto vertex_count_v = std::size_t{4};
constexpr auto index_count_v = std::size_t{6};

template <typename T>
constexpr auto vertex_view(T&& out, std::size_t start_index) {
	return std::span<std::remove_reference_t<decltype(out[0])>>{out}.subspan(start_index, vertex_count_v);
}

constexpr glm::vec2 indexed_uv_rect(std::size_t index) {
	switch (index) {
	case 0: return uv_rect_v.top_left();
	case 1: return uv_rect_v.top_right();
	case 2: return uv_rect_v.bottom_right();
	case 3: return uv_rect_v.bottom_left();
	default: break;
	}
	throw std::runtime_error{"Invalid UV index"};
}
} // namespace

Particle::Emitter::Emitter() : Emitter(Service<RenderDevice>::locate().vulkan_device()) {}

Particle::Emitter::Emitter(vulkan::Device& vulkan_device) : m_primitive(vulkan_device) {}

void Particle::Emitter::tick(glm::quat const& view, Duration dt) {
	if (config.count < m_particles.size()) {
		m_particles.resize(config.count);
		auto& geometry = m_primitive.geometry();
		resize(config.count * vertex_count_v, geometry.positions, geometry.normals, geometry.uvs, geometry.rgbas);
		resize(config.count * index_count_v, geometry.indices);
	} else if (config.count > m_particles.size()) {
		m_particles.reserve(config.count);
		auto& geometry = m_primitive.geometry();
		reserve(config.count * vertex_count_v, geometry.positions, geometry.normals, geometry.uvs, geometry.rgbas);
		reserve(config.count * index_count_v, geometry.indices);
		while (m_particles.size() < config.count) {
			m_particles.push_back(make_particle());
			append_quad(view, m_particles.back());
		}
	}
	for (auto [particle, index] : enumerate(m_particles)) {
		particle.elapsed += dt;
		if (particle.elapsed >= particle.ttl) { particle = make_particle(); }
		particle.alpha = std::clamp(particle.elapsed / particle.ttl, 0.0f, 1.0f);
		for (auto const& modifier : modifiers) {
			std::visit([p = &particle, dt](auto const& modifier) { modifier.tick(*p, dt); }, modifier);
		}
		mirror(particle, view, index * vertex_count_v);
	}
}

void Particle::Emitter::render(DrawList& out) const { out.add(&m_primitive, &material, Transform{}); }

auto Particle::Emitter::make_particle() const -> Particle {
	auto ret = Particle{};

	ret.velocity.linear = random_vec3(config.velocity.linear);
	ret.velocity.angular = util::random_range(config.velocity.angular.first.value, config.velocity.angular.second.value);

	ret.lerp.scale = config.lerp.scale;
	ret.lerp.tint = config.lerp.tint;

	ret.position = random_vec3(config.initial.position);
	ret.rotation = util::random_range(config.initial.rotation.first.value, config.initial.rotation.second.value);
	ret.scale = config.lerp.scale.first;

	ret.ttl = Duration{util::random_range(config.ttl.first.count(), config.ttl.second.count())};
	ret.tint = ret.lerp.tint.first;

	return ret;
}

void Particle::Emitter::mirror(Particle const& particle, glm::quat const& view, std::size_t const start_index) {
	auto const h = 0.5f * config.quad_size * particle.scale;

	auto& geometry = m_primitive.geometry();
	auto const positions = vertex_view(geometry.positions, start_index);
	positions[0] = {-h.x, +h.y, 0.0f};
	positions[1] = {+h.x, +h.y, 0.0f};
	positions[2] = {+h.x, -h.y, 0.0f};
	positions[3] = {-h.x, -h.y, 0.0f};

	for (auto& position : positions) {
		auto const orientation = view * glm::rotate(glm::identity<glm::quat>(), particle.rotation.value, front_v);
		position = orientation * position + particle.position;
	}

	auto const tint = Rgba::to_linear(particle.tint.to_vec4());
	auto const rgbas = vertex_view(geometry.rgbas, start_index);
	for (auto& rgba : rgbas) { rgba = tint; }
}

void Particle::Emitter::append_quad(glm::quat const& view, Particle const& particle) {
	auto& geometry = m_primitive.geometry();
	auto const index_offset = static_cast<std::uint32_t>(geometry.positions.size());

	for (std::size_t index = 0; index < vertex_count_v; ++index) {
		geometry.positions.push_back({});
		geometry.rgbas.push_back({});
		geometry.normals.push_back(front_v);
		geometry.uvs.push_back(indexed_uv_rect(index));
	}
	mirror(particle, view, static_cast<std::size_t>(index_offset));

	static constexpr std::uint32_t const indices[] = {
		0, 1, 2, 2, 3, 0,
	};
	std::ranges::transform(indices, std::back_inserter(geometry.indices), [index_offset](std::uint32_t const i) { return index_offset + i; });
}

void ParticleSystem::tick(Duration dt) {
	auto const& scene = *owning_scene();
	for (auto& emitter : emitters) { emitter.tick(scene.camera.transform.orientation(), dt); }
}

void ParticleSystem::render(DrawList& out) const {
	for (auto const& emitter : emitters) { emitter.render(out); }
}
} // namespace levk
