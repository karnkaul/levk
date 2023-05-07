#pragma once
#include <capo/device.hpp>
#include <capo/pcm.hpp>
#include <levk/util/not_null.hpp>
#include <levk/util/time.hpp>
#include <array>

namespace levk {
class Music {
  public:
	Music() = default;
	explicit Music(NotNull<capo::Device const*> device);

	void play(NotNull<capo::Pcm const*> pcm, Duration crossfade, float gain = 1.0f);

	void tick(Duration dt);

	explicit operator bool() const { return m_impl != nullptr; }

  private:
	struct Impl;
	struct Deleter {
		void operator()(Impl const* ptr) const;
	};
	std::unique_ptr<Impl, Deleter> m_impl{};
};
} // namespace levk
