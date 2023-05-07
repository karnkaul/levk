#include <levk/audio/sfx.hpp>
#include <deque>
#include <ranges>

namespace levk {
struct Sfx::Impl {
	struct Entry {
		capo::SoundSource source{};
		Id id{};
	};

	NotNull<capo::Device const*> audio_device;
	std::deque<Entry> entries{};
	Id prev_id{};
	std::size_t max_sources{};

	Impl(NotNull<capo::Device const*> audio_device, std::size_t max_sources) : audio_device(audio_device), max_sources(max_sources) {}
};

void Sfx::Deleter::operator()(Impl const* ptr) const { delete ptr; }

Sfx::Sfx(NotNull<capo::Device const*> audio_device, std::size_t max_sources) : m_impl(new Impl{audio_device, max_sources}) {}

auto Sfx::play(capo::Clip const& clip, glm::vec3 const& position, float gain) -> Id {
	if (!m_impl) { return {}; }
	auto const id = ++m_impl->prev_id;
	auto& source = next_source(id);
	source.set_clip(clip);
	source.set_gain(gain);
	source.set_position({position.x, position.y, position.z});
	source.play();
	return id;
}

bool Sfx::is_playing(Id id) const {
	if (!m_impl || !id) { return {}; }
	auto const it = std::ranges::find_if(m_impl->entries, [id](Impl::Entry const& e) { return e.id == id; });
	if (it == m_impl->entries.end()) { return false; }
	return it->source.state() == capo::State::ePlaying;
}

void Sfx::stop(Id id) {
	if (!m_impl || !id) { return; }
	auto const it = std::ranges::find_if(m_impl->entries, [id](Impl::Entry const& e) { return e.id == id; });
	if (it == m_impl->entries.end()) { return; }
	it->source.stop();
}

void Sfx::set_max_sources(std::size_t max_sources) {
	m_impl->max_sources = max_sources;
	while (m_impl->entries.size() > m_impl->max_sources) { m_impl->entries.pop_front(); }
}

std::size_t Sfx::max_sources() const {
	if (!m_impl) { return {}; }
	return m_impl->max_sources;
}

capo::SoundSource& Sfx::next_source(Id id) {
	auto ret = capo::SoundSource{};
	for (auto it = m_impl->entries.begin(); it != m_impl->entries.end(); ++it) {
		if (it->source.state() != capo::State::ePlaying) {
			ret = std::move(it->source);
			m_impl->entries.erase(it);
			break;
		}
	}
	if (!ret && m_impl->entries.size() >= m_impl->max_sources) {
		ret = std::move(m_impl->entries.front().source);
		m_impl->entries.pop_front();
	}
	if (!ret) { ret = m_impl->audio_device->make_sound_source(); }
	assert(ret);
	return m_impl->entries.emplace_back(Impl::Entry{std::move(ret), id}).source;
}
} // namespace levk
