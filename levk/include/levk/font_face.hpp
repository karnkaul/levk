#pragma once
#include <levk/glyph.hpp>
#include <memory>
#include <unordered_map>

namespace levk {
class FontFace {
  public:
	struct Key {
		Codepoint codepoint{Codepoint::eNull};
		TextHeight height{TextHeight::eDefault};

		bool operator==(Key const&) const = default;

		struct Hasher {
			std::size_t operator()(Key const& key) const;
		};
	};

	FontFace() = default;

	FontFace(std::unique_ptr<Glyph::Provider> glyph_provider) : m_provider(std::move(glyph_provider)) {}

	Glyph const& get(Key key);
	Glyph const& get(Codepoint codepoint) { return get(Key{codepoint}); }

	explicit operator bool() const { return m_provider != nullptr; }

  private:
	std::unordered_map<Key, Glyph, Key::Hasher> m_map{};
	std::unique_ptr<Glyph::Provider> m_provider{};
};
} // namespace levk
