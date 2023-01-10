#pragma once
#include <algorithm>
#include <span>
#include <vector>

namespace levk::imcpp {
template <typename Type>
class RingBuffer {
  public:
	RingBuffer(std::size_t capacity = 128) : m_capacity(capacity) {}

	void push(Type t) {
		if (m_buffer.size() >= m_capacity) {
			m_buffer.resize(m_capacity);
			std::rotate(m_buffer.begin(), m_buffer.begin() + 1, m_buffer.end());
			m_buffer.pop_back();
		}
		m_buffer.push_back(std::move(t));
	}

	void set_capacity(std::size_t capacity) { m_capacity = capacity; }
	std::size_t capacity() const { return m_capacity; }

	std::span<Type> span() { return m_buffer; }
	std::span<Type const> span() const { return m_buffer; }

  private:
	std::vector<Type> m_buffer{};
	std::size_t m_capacity;
};
} // namespace levk::imcpp
