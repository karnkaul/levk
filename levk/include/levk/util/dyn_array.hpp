#pragma once
#include <cassert>
#include <cstring>
#include <memory>
#include <span>

namespace levk {
///
/// \brief Basic dynamic array (wrapper over std::unique_ptr<T[]> + size).
///
template <typename T>
class DynArray {
  public:
	///
	/// \brief Default constructor (does nothing).
	///
	DynArray() = default;

	///
	/// \brief Construct a dynamic array of given size.
	/// \param size Desired size of dynamic array
	///
	explicit DynArray(std::size_t size) : m_data(std::make_unique<T[]>(size)), m_size(size) {}
	///
	/// \brief Transfer ownership of another dynamic array.
	/// \param data Data to transfer ownership of
	/// \param size Size of the data being transferred
	///
	explicit DynArray(std::unique_ptr<T[]>&& data, std::size_t size) : m_data(std::move(data)), m_size(size) {}

	///
	/// \brief Construct a dynamic array and populate it with the given data.
	/// \param data Data to copy
	///
	explicit DynArray(std::span<T const> data) : DynArray(data.size()) { std::memcpy(m_data.get(), data.data(), data.size()); }

	///
	/// \brief Obtain a pointer to the data.
	/// \returns Pointer to the stored data, else nullptr.
	///
	T* data() const { return m_data.get(); }
	///
	/// \brief Obtain the size of the stored data.
	/// \returns Size of the stored data.
	///
	std::size_t size() const { return m_data ? m_size : 0u; }
	///
	/// \brief Obtain a span into the stored data.
	/// \returns Span into stored data
	///
	std::span<T> span() const { return {m_data.get(), m_size}; }

	///
	/// \brief Obtain a reference to the object at index.
	/// \param index Index of array to subscript into
	/// \returns Reference to object at index
	///
	T& operator[](std::size_t index) const {
		assert(index < size());
		return m_data[index];
	}

	///
	/// \brief Check if instance has any data.
	/// \returns true if there is no data
	///
	bool empty() const { return m_size == 0u || !*this; }
	///
	/// \brief Check if instance is valid.
	/// \returns true if data is not null
	///
	explicit operator bool() const { return m_data != nullptr; }

  private:
	std::unique_ptr<T[]> m_data{};
	std::size_t m_size{};
};

using ByteArray = DynArray<std::byte>;
} // namespace levk
