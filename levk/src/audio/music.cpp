#include <levk/audio/music.hpp>
#include <levk/util/logger.hpp>

namespace levk {
namespace {
Logger const g_log{"Music"};
}

struct Music::Impl {
	struct Transition {
		Duration duration{};
		float source_gain{};
		float target_gain{1.0f};

		Duration elapsed{};

		explicit operator bool() const { return duration > 0s; }
	};

	std::array<capo::StreamSource, 2> sources{};
	std::size_t active_index{};
	Transition transition{};

	Impl(capo::Device const& device) {
		for (auto& source : sources) { source = device.make_stream_source(); }
	}

	capo::StreamSource& active() { return sources[active_index]; }
	capo::StreamSource const& active() const { return sources[active_index]; }

	capo::StreamSource& standby() { return sources[(active_index + 1) % sources.size()]; }
};

void Music::Deleter::operator()(Impl const* ptr) const { delete ptr; }

Music::Music(NotNull<capo::Device const*> device) : m_impl(new Impl{*device}) {}

void Music::play(NotNull<capo::Pcm const*> pcm, Duration crossfade, float gain) {
	if (!m_impl) { return; }
	if (crossfade <= 0s) {
		m_impl->active().set_stream(pcm->clip());
		m_impl->active().play();
		return;
	}
	m_impl->standby().set_stream(pcm->clip());
	m_impl->standby().set_gain(0.0f);
	m_impl->standby().play();
	m_impl->transition = Impl::Transition{
		.duration = crossfade,
		.source_gain = m_impl->active().gain(),
		.target_gain = gain,
	};
}

void Music::tick(Duration dt) {
	if (!m_impl || !m_impl->transition) { return; }
	m_impl->transition.elapsed += dt;

	auto& fading_out = m_impl->active();
	auto& fading_in = m_impl->standby();
	auto const ratio = m_impl->transition.elapsed / m_impl->transition.duration;
	if (ratio < 1.0f) {
		fading_in.set_gain(ratio * m_impl->transition.target_gain);
		fading_out.set_gain((1.0f - ratio) * m_impl->transition.source_gain);
	} else {
		fading_in.set_gain(m_impl->transition.target_gain);
		fading_out.stop();
		m_impl->active_index = (m_impl->active_index + 1) % m_impl->sources.size();
		m_impl->transition = {};
	}
}
} // namespace levk
