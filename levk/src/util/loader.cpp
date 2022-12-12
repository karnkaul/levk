#include <levk/util/error.hpp>
#include <levk/util/loader.hpp>
#include <levk/util/logger.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace levk {
namespace {
namespace fs = std::filesystem;

fs::path get_absolute_path(std::string_view const directory, std::string_view const uri) { return fs::absolute(fs::path{directory} / uri); }
} // namespace

FileLoader::FileLoader(std::string_view directory) {
	auto const path = fs::absolute(directory);
	if (!fs::is_directory(path)) {
		logger::warn("Path not a directory: [{}]", directory);
		return;
	}
	m_directory = path.generic_string();
}

FileLoader FileLoader::mount_parent_dir(std::string_view filename) { return FileLoader{fs::path{filename}.parent_path().generic_string()}; }

std::string FileLoader::absolute_path(std::string_view uri) const {
	auto const path = get_absolute_path(m_directory, uri);
	if (fs::exists(path)) { return path.generic_string(); }
	return {};
}

ByteArray FileLoader::load(std::string_view uri) const {
	auto const path = get_absolute_path(m_directory, uri);
	if (auto file = std::ifstream{path, std::ios::binary | std::ios::ate}) {
		auto const size = file.tellg();
		file.seekg({}, std::ios::beg);
		auto ret = ByteArray{static_cast<std::size_t>(size)};
		file.read(reinterpret_cast<char*>(ret.data()), size);
		return ret;
	}
	logger::warn("Failed to open file: [{}]", path.generic_string());
	return {};
}
} // namespace levk
