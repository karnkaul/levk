#pragma once
#include <fmt/format.h>

namespace levk {
///
/// \brief Simple "stringy" wrapper around a stack buffer of chars.
///
/// Stores a buffer of Capacity + 1 to guarantee null termination.
/// Note: chars in input strings beyond Capacity are silently dropped.
///
template <std::size_t Capacity = 64>
class FixedString {
  public:
	FixedString() = default;

	///
	/// \brief Construct an instance by copying t into the internal buffer.
	/// \param t Direct string to copy
	///
	template <std::convertible_to<std::string_view> T>
	FixedString(T const& t) {
		auto const str = std::string_view{t};
		m_size = str.size();
		std::memcpy(m_buffer, str.data(), m_size);
	}

	///
	/// \brief Construct an instance by formatting into the internal buffer.
	/// \param fmt Format string
	/// \param args... Format arguments
	///
	template <typename... Args>
	explicit FixedString(fmt::format_string<Args...> fmt, Args const&... args) {
		auto const [it, _] = fmt::vformat_to_n(m_buffer, Capacity, fmt, fmt::make_format_args(args...));
		m_size = static_cast<std::size_t>(std::distance(m_buffer, it));
	}

	///
	/// \brief Append a string to the internal buffer.
	/// \param rhs The string to append.
	///
	template <std::size_t N>
	void append(FixedString<N> const& rhs) {
		auto const dsize = std::min(Capacity - m_size, rhs.size());
		std::memcpy(m_buffer + m_size, rhs.data(), dsize);
		m_size += dsize;
	}

	///
	/// \brief Obtain a view into the stored string.
	/// \returns A view into the stored string
	///
	std::string_view view() const { return {m_buffer, m_size}; }
	///
	/// \brief Obtain a pointer to the start of the stored string.
	/// \returns A const pointer to the start of the string
	///
	char const* data() const { return m_buffer; }
	///
	/// \brief Obtain a pointer to the start of the stored string.
	/// \returns A const pointer to the start of the string
	///
	char const* c_str() const { return m_buffer; }
	///
	/// \brief Obtain the size of the stored string.
	/// \returns The number of characters in the string
	///
	std::size_t size() const { return m_size; }
	///
	/// \brief Check if the stored string is empty.
	/// \returns true If the size is 0
	///
	bool empty() const { return m_size == 0; }

	operator std::string_view() const { return view(); }

	template <std::size_t N>
	FixedString& operator+=(FixedString<N> const& rhs) {
		append(rhs);
		return *this;
	}

	bool operator==(FixedString const& rhs) const { return view() == rhs.view(); }

  private:
	char m_buffer[Capacity + 1]{};
	std::size_t m_size{};
};
} // namespace levk
