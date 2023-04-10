#include <levk/util/async_queue.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/ptr.hpp>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
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

class FileLogger : public Logger::Sink {
  public:
	FileLogger(char const* path) : m_path(fs::absolute(path)), m_thread(std::jthread{[this](std::stop_token const& stop) { log_to_file(stop); }}) {}

	~FileLogger() override {
		m_thread.request_stop();
		flush_residue(m_queue.release());
	}

  private:
	void prepare_file() {
		if (fs::exists(m_path)) {
			auto backup_path = m_path;
			backup_path += ".bak";
			if (fs::exists(backup_path)) { fs::remove(backup_path); }
			fs::rename(m_path, backup_path);
		}
	}

	void flush_residue(std::deque<Logger::Entry> const& residue) {
		auto file = std::ofstream{m_path, std::ios::app};
		if (!file) { return; }
		for (auto const& [message, _] : residue) { file << message << '\n'; }
	}

	void log_to_file(std::stop_token const& stop) {
		prepare_file();
		while (auto entry = m_queue.pop(stop)) {
			auto file = std::ofstream{m_path, std::ios::app};
			file << entry->formatted_message << '\n';
		}
	}

	void init(std::span<Logger::Entry const> entries) final { m_queue.push(entries.begin(), entries.end()); }
	void on_log(Logger::Entry const& entry) final { m_queue.push(entry); }

	fs::path m_path{};
	std::jthread m_thread{};
	AsyncQueue<Logger::Entry> m_queue{};
};

struct Storage {
	struct Buffer {
		Logger::BufferSize size{};

		std::vector<Logger::Entry> entries{};

		void push(Logger::Entry entry) {
			entries.push_back(std::move(entry));
			if (entries.size() > size.total()) {
				auto const new_begin = entries.begin() + static_cast<std::ptrdiff_t>(size.delta);
				std::rotate(entries.begin(), new_begin, entries.end());
				entries.resize(size.limit);
			}
		}
	};

	Buffer buffer{};
	std::optional<FileLogger> file_logger{};
	std::unordered_set<Ptr<Logger::Sink>> sinks{};
	std::mutex mutex{};
};

GetThreadId get_thread_id{};
Storage g_storage{};
} // namespace

Logger::Sink::~Sink() {
	auto lock = std::scoped_lock{g_storage.mutex};
	g_storage.sinks.erase(this);
}

int Logger::thread_id() { return get_thread_id(); }

Logger::BufferSize Logger::buffer_size() { return g_storage.buffer.size; }

void Logger::access_buffer(Accessor& accessor) {
	auto lock = std::scoped_lock{g_storage.mutex};
	accessor(g_storage.buffer.entries);
}

void Logger::attach(Sink& out_sink) {
	auto lock = std::scoped_lock{g_storage.mutex};
	g_storage.sinks.insert(&out_sink);
	out_sink.init(g_storage.buffer.entries);
}

Logger::Instance::Instance(char const* file_path, Logger::BufferSize const& buffer_size) {
	auto lock = std::scoped_lock{g_storage.mutex};
	g_storage.buffer = Storage::Buffer{buffer_size};
	g_storage.file_logger.emplace(file_path);
}

Logger::Instance::~Instance() {
	g_storage.file_logger.reset();
	g_storage.buffer.entries.clear();
}

std::string Logger::format(Level level, std::string_view context, std::string_view const message) {
	if (context.empty()) { context = "Unknown"; }
	return fmt::format(fmt::runtime(s_format), fmt::arg("thread", thread_id()), fmt::arg("level", levels_v[level]), fmt::arg("context", context),
					   fmt::arg("message", message), fmt::arg("timestamp", make_timestamp()));
}

void Logger::print_to(Pipe pipe, Entry entry) {
	auto* fd = pipe == Pipe::eStdErr ? stderr : stdout;
	std::fprintf(fd, "%s\n", entry.formatted_message.c_str());
	auto lock = std::scoped_lock{g_storage.mutex};
#if defined(_WIN32)
	OutputDebugStringA(entry.message.c_str());
	OutputDebugStringA("\n");
#endif
	for (auto const& sink : g_storage.sinks) { sink->on_log(entry); }
	g_storage.buffer.push(std::move(entry));
}
} // namespace levk
