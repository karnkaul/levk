#pragma once
#include <capo/device.hpp>
#include <glm/vec3.hpp>
#include <levk/util/not_null.hpp>
#include <memory>

namespace levk {
class Sfx {
  public:
	using Id = std::uint64_t;

	Sfx() = default;
	explicit Sfx(NotNull<capo::Device const*> device, std::size_t max_sources = 32);

	Id play(capo::Clip const& clip, glm::vec3 const& position = {}, float gain = 1.0f);
	bool is_playing(Id id) const;
	void stop(Id id);

	void set_max_sources(std::size_t max_sources);
	std::size_t max_sources() const;

	explicit operator bool() const { return m_impl != nullptr; }

  private:
	capo::SoundSource& next_source(Id id);

	struct Impl;
	struct Deleter {
		void operator()(Impl const* ptr) const;
	};
	std::unique_ptr<Impl, Deleter> m_impl{};
};
} // namespace levk
