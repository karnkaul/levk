#pragma once
#include <string>

namespace levk {
template <typename Type = void>
class Uri;

template <>
class Uri<void> {
  public:
	struct Builder;
	struct Hasher;

	Uri() = default;

	Uri(std::string value);

	Uri(char const* value) : Uri(std::string{value}) {}
	Uri(std::string_view value) : Uri(std::string{value}) {}

	std::string const& value() const { return m_value; }
	std::size_t hash() const { return m_hash; }

	bool is_empty() const { return m_value.empty() || m_hash == 0; }
	explicit operator bool() const { return !is_empty(); }

	bool operator==(Uri const& rhs) const { return m_hash == rhs.m_hash; }

	std::string absolute_path(std::string_view root_dir) const;
	std::string parent() const;
	std::string append(std::string_view suffix) const;
	std::string concat(std::string_view suffix) const;

  private:
	Uri(std::string value, std::size_t hash) : m_value(std::move(value)), m_hash(hash) {}

	std::string m_value{};
	std::size_t m_hash{};

	template <typename T>
	friend class Uri;
};

template <typename Type>
class Uri : public Uri<> {
  public:
	using Uri<>::Uri;

	Uri(Uri<> uri) : Uri(std::move(uri.m_value), uri.m_hash) {}
};

struct Uri<>::Hasher {
	std::size_t operator()(Uri<> const& uri) const { return uri.hash(); }
};

struct Uri<>::Builder {
	std::string root_dir{};
	std::string sub_dir{};

	std::string build(std::string_view uri) const;
	std::string operator()(std::string_view uri) const { return build(uri); }
};
} // namespace levk
