#pragma once
#include <cstring>
#include <fstream>
#include <span>
#include <vector>

namespace levk {
template <typename Type>
concept BinaryDataT = (std::is_trivially_copyable_v<Type> and not std::is_pointer_v<Type>);

template <typename FileType>
class BinaryFile {
  public:
	BinaryFile(char const* path) : m_file(path, std::ios::binary) {}

	explicit operator bool() const { return static_cast<bool>(m_file); }

  protected:
	FileType m_file{};
};

class BinaryOutFile : public BinaryFile<std::ofstream> {
  public:
	using BinaryFile::BinaryFile;

	template <BinaryDataT T>
	void write(std::span<T const> ts) {
		auto const data = std::span{reinterpret_cast<char const*>(ts.data()), ts.size_bytes()};
		m_file.write(data.data(), static_cast<std::streamsize>(data.size()));
	}
};

class BinaryInFile : public BinaryFile<std::ifstream> {
  public:
	using BinaryFile::BinaryFile;

	template <BinaryDataT T>
	void read(std::span<T> out) {
		m_buffer.resize(out.size_bytes());
		m_file.read(reinterpret_cast<char*>(m_buffer.data()), static_cast<std::streamsize>(out.size_bytes()));
		std::memcpy(out.data(), m_buffer.data(), out.size_bytes());
	}

	template <BinaryDataT T>
	void read(T& out) {
		std::byte buffer[sizeof(T)]{};
		m_file.read(reinterpret_cast<char*>(buffer), static_cast<std::streamsize>(sizeof(T)));
		std::memcpy(&out, buffer, sizeof(T));
	}

  private:
	std::vector<std::byte> m_buffer{};
};
} // namespace levk
