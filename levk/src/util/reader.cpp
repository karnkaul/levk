#include <levk/util/enumerate.hpp>
#include <levk/util/hash_combine.hpp>
#include <levk/util/loader.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/reader.hpp>
#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace levk {
namespace fs = std::filesystem;

std::size_t Reader::compute_hash(std::span<std::byte const> bytes) {
	auto ret = std::size_t{};
	for (auto const byte : bytes) { hash_combine(ret, byte); }
	return ret;
}

struct FileReader::Impl {
	struct Entry {
		ByteArray data{};
		std::size_t loader{};
		std::size_t hash{};
		fs::file_time_type lwt{};
	};

	std::vector<FileLoader> mounted{};
	std::unordered_map<std::string, Entry> loaded{};
	std::mutex mutex{};
};

FileReader::FileReader() : m_impl(std::make_unique<Impl>()) {}
FileReader::FileReader(FileReader&&) noexcept = default;
FileReader& FileReader::operator=(FileReader&&) noexcept = default;
FileReader::~FileReader() = default;

std::string FileReader::absolute_path(std::string_view const uri) const {
	for (auto const& provider : m_impl->mounted) {
		if (auto ret = provider.absolute_path(uri); !ret.empty()) { return ret; }
	}
	return {};
}

bool FileReader::mount(std::string_view directory) {
	if (auto loader = FileLoader{directory}) {
		auto lock = std::scoped_lock{m_impl->mutex};
		m_impl->mounted.push_back(std::move(loader));
		logger::info("[FileReader] Directory [{}] mounted successfully", directory);
		return true;
	}
	return false;
}

FileReader::Data FileReader::load(std::string const& uri, std::uint8_t flags) {
	auto lock = std::unique_lock{m_impl->mutex};
	if (auto it = m_impl->loaded.find(uri); it != m_impl->loaded.end()) {
		auto& entry = it->second;
		auto& loader = m_impl->mounted[entry.loader];
		lock.unlock();
		if ((flags & eReload)) {
			entry.data = loader.load(uri);
			if (!entry.data) { return reload(uri, flags); }
			entry.lwt = fs::last_write_time(loader.absolute_path(uri));
			if (flags & eComputeHash) { entry.hash = compute_hash(entry.data.span()); }
			if (!(flags & eSilent)) { logger::info("[FileReader] (Re)loaded URI: [{}]", uri); }
		}
		if (!entry.hash && (flags & eComputeHash)) { entry.hash = compute_hash(entry.data.span()); }
		return {entry.data.span(), entry.hash};
	}
	lock.unlock();
	return reload(uri, flags);
}

FileReader::Data FileReader::reload(std::string const& uri, std::uint8_t flags) {
	auto lock = std::scoped_lock{m_impl->mutex};
	for (auto const& [loader, index] : enumerate(m_impl->mounted)) {
		auto data = loader.load(uri);
		if (data) {
			auto const lwt = fs::last_write_time(loader.absolute_path(uri));
			auto const hash = (flags & eComputeHash) ? compute_hash(data.span()) : 0;
			auto [it, _] = m_impl->loaded.insert_or_assign(uri, Impl::Entry{std::move(data), index, hash, lwt});
			if (!(flags & eSilent)) { logger::info("[FileReader] Loaded URI: [{}]", uri); }
			return {it->second.data.span(), it->second.hash};
		}
	}
	if (!(flags & eSilent)) { logger::warn("[FileReader] Failed to loaded URI: [{}]", uri); }
	return {};
}

std::uint32_t FileReader::reload_out_of_date(bool silent) {
	auto ret = std::uint32_t{};
	auto lock = std::scoped_lock{m_impl->mutex};
	for (auto& [uri, entry] : m_impl->loaded) {
		auto const& loader = m_impl->mounted[entry.loader];
		if (entry.lwt < fs::last_write_time(loader.absolute_path(uri))) {
			entry.data = loader.load(uri);
			if (entry.hash) { entry.hash = compute_hash(entry.data.span()); }
			if (!silent) { logger::info("[FileReader] Reloaded out of date URI: [{}]", uri); }
			++ret;
		}
	}
	return ret;
}
} // namespace levk
