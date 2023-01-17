#pragma once
#include <cassert>
#include <cstring>
#include <fstream>
#include <span>
#include <vector>

namespace levk {
template <typename Type>
concept BinaryDataT = (std::is_trivially_copyable_v<Type> and not std::is_pointer_v<Type>);

class BinarySourceFile {
  public:
	BinarySourceFile(char const* path) : m_file(path, std::ios::binary) {}

	void read_to(std::byte* out_data, std::size_t size) { m_file.read(reinterpret_cast<char*>(out_data), static_cast<std::streamsize>(size)); }

	explicit operator bool() const { return static_cast<bool>(m_file); }

  private:
	std::ifstream m_file{};
};

class BinarySourceData {
  public:
	BinarySourceData(std::span<std::byte const> data) : m_data(data) {}

	void read_to(std::byte* out_data, std::size_t size) {
		assert(m_data.size() >= size);
		std::memcpy(out_data, m_data.data(), size);
		m_data = m_data.subspan(size);
	}

	explicit operator bool() const { return !m_data.empty(); }

  private:
	std::span<std::byte const> m_data{};
};

class BinaryOutFile {
  public:
	BinaryOutFile(char const* path) : m_file(path, std::ios::binary) {}

	template <BinaryDataT T>
	void write(std::span<T const> ts) {
		auto const data = std::span{reinterpret_cast<char const*>(ts.data()), ts.size_bytes()};
		m_file.write(data.data(), static_cast<std::streamsize>(data.size()));
	}

	explicit operator bool() const { return static_cast<bool>(m_file); }

  private:
	std::ofstream m_file{};
};

template <typename Type>
concept BinarySourceT = requires(Type& t, std::byte* b, std::size_t s) { t.read_to(b, s); };

template <BinarySourceT SourceT>
class BinaryIn {
  public:
	BinaryIn(SourceT source) : m_source(std::move(source)) {}

	template <BinaryDataT T>
	void read(std::span<T> out) {
		m_buffer.resize(out.size_bytes());
		m_source.read_to(m_buffer.data(), out.size_bytes());
		std::memcpy(out.data(), m_buffer.data(), out.size_bytes());
	}

	template <BinaryDataT T>
	void read(T& out) {
		std::byte buffer[sizeof(T)]{};
		m_source.read_to(buffer, sizeof(T));
		std::memcpy(&out, buffer, sizeof(T));
	}

	explicit operator bool() const { return static_cast<bool>(m_source); }

  private:
	SourceT m_source{};
	std::vector<std::byte> m_buffer{};
};
} // namespace levk
