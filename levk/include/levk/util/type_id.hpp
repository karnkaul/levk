#pragma once
#include <atomic>
#include <cstdint>

namespace levk {
///
/// \brief Maps a unique identifier for each instantiating type.
///
class TypeId {
  public:
	///
	/// \brief Obtain (or create) a unique TypeId instance for Type.
	/// \param Type The type to instantiate the unique Id for
	///
	template <typename Type>
	static TypeId make() {
		return get_id<Type>();
	}

	TypeId() = default;

	///
	/// \brief Obtain the underlying Id.
	/// \returns The underlying id
	///
	std::size_t value() const { return m_id; }
	explicit operator std::size_t() const { return value(); }
	bool operator==(TypeId const&) const = default;

  private:
	TypeId(std::size_t id) : m_id(id) {}

	template <typename T>
	static std::size_t get_id() {
		static auto const ret{++s_next_id};
		return ret;
	}

	inline static std::atomic<std::size_t> s_next_id{};
	std::size_t m_id{};
};
} // namespace levk
