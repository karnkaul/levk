#pragma once
#include <string>

namespace levk {
class Uri {
  public:
	struct Hasher;
	struct Builder;

	Uri() = default;

	Uri(std::string value);

	Uri(char const* value) : Uri(std::string{value}) {}
	Uri(std::string_view value) : Uri(std::string{value}) {}

	std::string_view value() const { return m_value; }
	std::size_t hash() const { return m_hash; }

	explicit operator bool() const { return !m_value.empty() && m_hash > 0; }

	bool operator==(Uri const& rhs) const { return m_hash == rhs.m_hash; }

	std::string absolute_path(std::string_view root_dir) const;
	std::string parent() const;
	std::string append(std::string_view suffix) const;
	std::string concat(std::string_view suffix) const;

  private:
	std::string m_value{};
	std::size_t m_hash{};
};

struct Uri::Hasher {
	std::size_t operator()(Uri const& uri) const { return uri.hash(); }
};

struct Uri::Builder {
	std::string root_dir{};
	std::string sub_dir{};

	std::string build(std::string_view uri) const;
	std::string operator()(std::string_view uri) const { return build(uri); }
};

template <typename Type>
using TUri = Uri;
} // namespace levk
