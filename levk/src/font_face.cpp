#include <levk/font_face.hpp>
#include <levk/util/hash_combine.hpp>

namespace levk {
std::size_t FontFace::Key::Hasher::operator()(Key const& key) const { return make_combined_hash(key.codepoint, key.height); }

Glyph const& FontFace::get(Key const key) {
	auto it = m_map.find(key);
	if (it == m_map.end() && m_provider) {
		m_provider->set_height(key.height);
		auto [i, _] = m_map.insert_or_assign(key, m_provider->glyph_for(key.codepoint));
		it = i;
	}
	if (it == m_map.end()) {
		static auto const s_null{Glyph{}};
		return s_null;
	}
	return it->second;
}
} // namespace levk
