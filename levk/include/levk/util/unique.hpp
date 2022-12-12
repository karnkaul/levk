#pragma once
#include <concepts>
#include <utility>

namespace levk {
///
/// \brief Default deleter for a Unique<Type>.
///
template <typename Type>
struct DefaultDeleter {
	constexpr void operator()(Type) const {}
};

///
/// \brief Wrapper for a unique instance of Type, with an accompanying Deleter.
///
template <typename Type, typename Deleter = DefaultDeleter<Type>>
class Unique {
  public:
	///
	/// \brief Construct a Unique instance.
	/// \param t The Object to move and store
	/// \param deleter The deleter to move and store
	///
	constexpr Unique(Type t = {}, Deleter deleter = {}) : m_t{std::move(t)}, m_deleter{std::move(deleter)} {}

	constexpr Unique(Unique&& rhs) noexcept : Unique() { swap(rhs); }
	constexpr Unique& operator=(Unique rhs) noexcept { return (swap(rhs), *this); }
	constexpr ~Unique() noexcept { m_deleter(std::move(m_t)); }

	constexpr void swap(Unique& rhs) noexcept {
		using std::swap;
		swap(m_t, rhs.m_t);
		swap(m_deleter, rhs.m_deleter);
	}

	///
	/// \brief Obtain a mutable reference to the stored object.
	/// \returns A mutable reference to the stored object.
	///
	constexpr Type& get() { return m_t; }
	///
	/// \brief Obtain an immutable reference to the stored object.
	/// \returns An immutable reference to the stored object.
	///
	constexpr Type const& get() const { return m_t; }

	///
	/// \brief Check if the stored object is not default constructed (if comparable).
	/// \returns true If the stored object compares equal to a default constructed one
	///
	explicit constexpr operator bool() const
		requires(std::equality_comparable<Type>)
	{
		return m_t != Type{};
	}

	constexpr bool operator==(Unique<Type, Deleter> const& rhs) const
		requires(std::equality_comparable<Type>)
	{
		return !(*this) && !rhs;
	}

	constexpr operator Type&() { return get(); }
	constexpr operator Type const&() const { return get(); }

	constexpr Type& operator->() { return get(); }
	constexpr Type const& operator->() const { return get(); }

	constexpr Deleter& get_deleter() { return m_deleter; }
	constexpr Deleter const& get_deleter() const { return m_deleter; }

  private:
	Type m_t{};
	[[no_unique_address]] Deleter m_deleter{};
};
} // namespace levk
