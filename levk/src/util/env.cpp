#include <levk/util/env.hpp>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <mutex>

#if defined(_WIN32)
#include <ShlObj.h>
#include <Windows.h>
#endif

namespace {
namespace fs = std::filesystem;
using Lock = std::scoped_lock<std::mutex>;

std::mutex g_mutex{};

#if defined(_WIN32)
struct PtrDeleter {
	void operator()(LPVOID p) const { CoTaskMemFree(p); }
};

std::string get_known_folder_path(Lock const&, KNOWNFOLDERID id) {
	PWSTR ptr;
	HRESULT result = SHGetKnownFolderPath(id, {}, {}, &ptr);
	auto path = std::unique_ptr<WCHAR, PtrDeleter>{ptr};
	if (result == S_OK) { return fs::absolute(path.get()).generic_string(); }
	return {};
}
#endif

std::string get_home_path(char const* suffix) {
	if (auto const* home = std::getenv("HOME")) {
		auto const docs = fs::absolute(home) / suffix;
		if (fs::is_directory(docs)) { return docs.generic_string(); }
	}
	return {};
}
} // namespace

namespace levk {
std::string env::to_filename(std::string_view path) { return fs::path{path}.filename().generic_string(); }

std::string env::documents_path() {
	auto ret = std::string{};
	auto lock = std::scoped_lock{g_mutex};
#if defined(_WIN32)
	ret = get_known_folder_path(lock, FOLDERID_Documents);
#elif defined(__linux__)
	ret = get_home_path("Documents");
#endif
	return ret;
}

std::string env::downloads_path() {
	auto ret = std::string{};
	auto lock = std::scoped_lock{g_mutex};
#if defined(_WIN32)
	ret = get_known_folder_path(lock, FOLDERID_Downloads);
#elif defined(__linux__)
	ret = get_home_path("Downloads");
#endif
	return ret;
}

bool env::GetDirEntries::operator()(DirEntries& out_entries, char const* path) const {
	if (!fs::is_directory(path)) { return false; }
	auto const extension_match = [extensions = extensions](std::string_view const ext) {
		if (extensions.empty()) { return true; }
		for (auto const match : extensions) {
			if (ext == match) { return true; }
		}
		return std::any_of(extensions.begin(), extensions.end(), [ext](std::string_view const str) { return ext == str; });
	};
	out_entries.dirs.clear();
	out_entries.files.clear();
	for (auto const& it : fs::directory_iterator{path}) {
		auto const& path = it.path();
		if (it.is_directory()) {
			if (!show_hidden && path.filename().string().starts_with(".")) { continue; }
			out_entries.dirs.push_back(path.generic_string());
		}
		if (it.is_regular_file() && extension_match(path.extension().string())) {
			if (!extension_match(path.extension().string())) { continue; }
			if (!show_hidden && path.filename().string().starts_with(".")) { continue; }
			out_entries.files.push_back(path.generic_string());
		}
	}

	if (sort) {
		std::sort(out_entries.dirs.begin(), out_entries.dirs.end());
		std::sort(out_entries.files.begin(), out_entries.files.end());
	}
	return true;
}
} // namespace levk
