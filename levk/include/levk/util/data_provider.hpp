#pragma once
#include <levk/util/dyn_array.hpp>
#include <string>

namespace levk {
///
/// \brief Pure virtual interface for a data provider that returns bytes given a (relative) URI.
///
struct DataProvider {
	virtual ~DataProvider() = default;
	///
	/// \brief Load the data located at uri.
	/// \param uri The URI to load
	/// \returns An empty ByteArray if not found
	///
	virtual ByteArray load(std::string_view uri) const = 0;
};

///
/// \brief Concrete DataProvider that uses the filesystem.
///
class FileDataProvider : public DataProvider {
  public:
	///
	/// \brief Create an instance with the parent directory of filename mounted as the root / prefix.
	/// \param filename (Absolute or relative) path to the filename whose parent directory to mount
	/// \returns FileDataProvider instance
	///
	static FileDataProvider mount_parent_dir(std::string_view filename);

	///
	/// \brief Construct an instance with directory mounted as the root / prefix.
	/// \param directory (Absolute or relative) path to the directory to mount
	///
	FileDataProvider(std::string_view directory);

	ByteArray load(std::string_view uri) const override;

	///
	/// \brief Check if this instance has a root / prefix mounted.
	/// \returns true If a root / prefix is mounted.
	///
	explicit operator bool() const { return !m_directory.empty(); }

  private:
	std::string m_directory{};
};
} // namespace levk
