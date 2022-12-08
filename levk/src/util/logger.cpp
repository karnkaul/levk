#include <levk/util/async_queue.hpp>
#include <levk/util/logger.hpp>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace levk {
namespace {
namespace fs = std::filesystem;

struct Timestamp {
	char buffer[32]{};
	operator char const*() const { return buffer; }
};

Timestamp make_timestamp() {
	auto const now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	auto ret = Timestamp{};
	if (!std::strftime(ret.buffer, sizeof(ret.buffer), "%H:%M:%S", std::localtime(&now))) { return {}; }
	return ret;
}

struct GetThreadId {
	std::unordered_map<std::thread::id, int> ids{};
	int next{};
	std::mutex mutex{};

	int operator()() {
		auto lock = std::scoped_lock{mutex};
		auto const tid = std::this_thread::get_id();
		if (auto const it = ids.find(tid); it != ids.end()) { return it->second; }
		auto const [it, _] = ids.insert_or_assign(tid, next++);
		return it->second;
	}
};

class FileLogger : Pinned {
  public:
	FileLogger(char const* path) : m_path(fs::absolute(path)), m_thread(std::jthread{[this](std::stop_token const& stop) { log_to_file(stop); }}) {}

	~FileLogger() {
		m_thread.request_stop();
		flush_residue(queue.release());
	}

	AsyncQueue<logger::Entry> queue{};

  private:
	void prepare_file() {
		if (fs::exists(m_path)) {
			auto backup_path = m_path;
			backup_path += ".bak";
			if (fs::exists(backup_path)) { fs::remove(backup_path); }
			fs::rename(m_path, backup_path);
		}
	}

	void flush_residue(std::deque<logger::Entry> const& residue) {
		auto file = std::ofstream{m_path, std::ios::app};
		if (!file) { return; }
		for (auto const& [message, _] : residue) { file << message << '\n'; }
	}

	void log_to_file(std::stop_token const& stop) {
		prepare_file();
		while (auto entry = queue.pop(stop)) {
			auto file = std::ofstream{m_path, std::ios::app};
			file << entry->message << '\n';
		}
	}

	fs::path m_path{};
	std::jthread m_thread{};
};

struct Storage {
	struct Buffer {
		logger::BufferSize size{};

		std::vector<logger::Entry> entries{};
	};

	Buffer buffer{};
	std::optional<FileLogger> file_logger{};
	std::mutex mutex{};
};

GetThreadId get_thread_id{};
Storage g_storage{};
} // namespace

int logger::thread_id() { return get_thread_id(); }

std::string logger::format(Level level, std::string_view const message) {
	return fmt::format(fmt::runtime(g_format), fmt::arg("thread", thread_id()), fmt::arg("level", levels_v[static_cast<std::size_t>(level)]),
					   fmt::arg("message", message), fmt::arg("timestamp", make_timestamp()));
}

void logger::log_to(Pipe pipe, Entry entry) {
	auto* fd = pipe == Pipe::eStdErr ? stderr : stdout;
	std::fprintf(fd, "%s\n", entry.message.c_str());
	auto lock = std::scoped_lock{g_storage.mutex};
#if defined(_WIN32)
	OutputDebugStringA(entry.message.c_str());
	OutputDebugStringA("\n");
#endif
	if (g_storage.file_logger) { g_storage.file_logger->queue.push(entry); }
	g_storage.buffer.entries.push_back(std::move(entry));
}

logger::BufferSize const& logger::buffer_size() { return g_storage.buffer.size; }

void logger::access_buffer(Accessor& accessor) {
	auto lock = std::scoped_lock{g_storage.mutex};
	accessor(g_storage.buffer.entries);
}

logger::Instance::Instance(logger::BufferSize const& buffer_size) {
	auto lock = std::scoped_lock{g_storage.mutex};
	g_storage.buffer = Storage::Buffer{buffer_size};
	g_storage.file_logger.emplace("levk.log");
}

logger::Instance::~Instance() {
	auto lock = std::scoped_lock{g_storage.mutex};
	g_storage.file_logger.reset();
	g_storage.buffer.entries.clear();
}
} // namespace levk
