#pragma once
#include <fmt/format.h>
#include <levk/defines.hpp>
#include <levk/util/enum_array.hpp>
#include <levk/util/pinned.hpp>
#include <span>

namespace levk {
class Logger {
  public:
	///
	/// \brief The output pipe for logging.
	///
	enum class Pipe : std::uint8_t { eStdOut, eStdErr };
	///
	/// \brief The level of a log message.
	///
	enum class Level : std::uint8_t { eError, eWarn, eInfo, eDebug, eCOUNT_ };
	static constexpr auto levels_v{EnumArray<Level, char>{'E', 'W', 'I', 'D'}};

	///
	/// \brief The format for a log message.
	///
	inline static std::string s_format{"[T{thread}] [{level}] [{context}] {message} [{timestamp}]"};

	///
	/// \brief A log message entry.
	///
	struct Entry {
		std::string formatted_message{};
		Level level{};
	};

	struct BufferSize;
	struct Accessor;
	class Instance;
	struct Sink;

	///
	/// \brief Obtain the current thread ID.
	/// \returns The thread ID
	///
	static int thread_id();

	///
	/// \brief Obtain the log buffer size.
	/// \returns The log buffer size.
	///
	static BufferSize buffer_size();
	///
	/// \brief Access the log buffer.
	/// \param accessor Concrete sub-type of Accessor
	///
	static void access_buffer(Accessor& accessor);
	///
	/// \brief Attach a sink to receive log callbacks.
	///
	/// Sink's destructor will detach itself.
	///
	static void attach(Sink& out_sink);

	///
	/// \brief Log an entry through a pipe.
	/// \param pipe The pipe to log the text through
	/// \param entry The log message entry
	///
	static void print_to(Pipe pipe, Entry entry);

	///
	/// \brief Format a log message given the level.
	/// \param level The level of this log message
	/// \param message The log message text
	/// \returns Text formatted according to g_format
	///
	static std::string format(Level level, std::string_view context, std::string_view message);

	///
	/// \brief Format a log message.
	/// \param level The log level
	/// \param fmt Format string
	/// \param args... Format arguments
	/// \returns String formatted according to g_format
	///
	template <typename... Args>
	static std::string format(Level level, std::string_view const context, fmt::format_string<Args...> fmt, Args const&... args) {
		return format(level, context, fmt::vformat(fmt, fmt::make_format_args(args...)));
	}

	virtual ~Logger() = default;

	Logger(std::string_view context = "General") : context(context) {}

	///
	/// \brief Log an error message.
	/// \param fmt Format string
	/// \param args... Format arguments
	///
	template <typename... Args>
	void error(fmt::format_string<Args...> fmt, Args const&... args) const {
		if (silent[Level::eError]) { return; }
		print(Entry{format(Level::eError, context, fmt, args...), Level::eError});
	}

	///
	/// \brief Log a warning message.
	/// \param fmt Format string
	/// \param args... Format arguments
	///
	template <typename... Args>
	void warn(fmt::format_string<Args...> fmt, Args const&... args) const {
		if (silent[Level::eWarn]) { return; }
		print(Entry{format(Level::eWarn, context, fmt, args...), Level::eWarn});
	}

	///
	/// \brief Log an info message.
	/// \param fmt Format string
	/// \param args... Format arguments
	///
	template <typename... Args>
	void info(fmt::format_string<Args...> fmt, Args const&... args) const {
		if (silent[Level::eInfo]) { return; }
		print(Entry{format(Level::eInfo, context, fmt, args...), Level::eInfo});
	}

	///
	/// \brief Log a debug message.
	/// \param fmt Format string
	/// \param args... Format arguments
	///
	template <typename... Args>
	void debug(fmt::format_string<Args...> fmt, Args const&... args) const {
		if constexpr (debug_v) {
			if (silent[Level::eDebug]) { return; }
			print(Entry{format(Level::eDebug, context, fmt, args...), Level::eDebug});
		}
	}

	///
	/// \brief Log an entry.
	/// \param entry Entry to log
	///
	void print(Entry entry) const { print_to(entry.level == Level::eError ? Pipe::eStdErr : Pipe::eStdOut, std::move(entry)); }

	///
	/// \brief Log context for this instance.
	///
	std::string_view context{};
	///
	/// \brief Levels that should be suppressed from being logged.
	///
	EnumArray<Level, bool> silent{};
};

///
/// \brief Size of the buffer for stored logs.
///
struct Logger::BufferSize {
	///
	/// \brief Lower threshold: will shrink down to this size.
	///
	std::size_t limit{500};
	///
	/// \brief Extra space beyond lower threshold: crossing this will trigger a shrink.
	///
	std::size_t delta{100};

	///
	/// \brief Total size of the buffer.
	///
	constexpr std::size_t total() const { return limit + delta; }
};

///
/// \brief Pure virtual interface for concurrent access to the entries stored in the log buffer.
///
struct Logger::Accessor {
	virtual void operator()(std::span<Entry const> entries) = 0;
};

struct Logger::Sink : Pinned {
	virtual ~Sink();

	virtual void init(std::span<Entry const> backlog) = 0;
	virtual void on_log(Entry const& entry) = 0;
};

///
/// \brief RAII instance that initializes and empties the buffer.
///
class Logger::Instance : public Pinned {
  public:
	///
	/// \brief Initialize the log buffer.
	/// \param buffer_size Desired size of the log buffer
	///
	Instance(char const* file_path = "levk.log", BufferSize const& buffer_size = {});
	~Instance();
};

///
/// \brief Default Logger instance.
///
inline auto const g_logger{Logger{}};
} // namespace levk
