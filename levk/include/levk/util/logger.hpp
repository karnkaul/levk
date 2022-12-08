#pragma once
#include <fmt/format.h>
#include <levk/defines.hpp>
#include <levk/util/pinned.hpp>
#include <span>

namespace levk::logger {
///
/// \brief The output pipe for logging.
///
enum class Pipe : std::uint8_t { eStdOut, eStdErr };
///
/// \brief The level of a log message.
///
enum class Level : std::uint8_t { eError, eWarn, eInfo, eDebug, eCOUNT_ };
inline constexpr char levels_v[] = {'E', 'W', 'I', 'D'};

///
/// \brief The format for a log message.
///
inline std::string g_format{"[T{thread}] [{level}] {message} [{timestamp}]"};

///
/// \brief A log message entry.
///
struct Entry {
	std::string message{};
	Level level{};
};

///
/// \brief Size of the buffer for stored logs.
///
struct BufferSize {
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
/// \brief Pure virtual interface for concurrent access to the entries stored in the log buffer
///
struct Accessor {
	virtual void operator()(std::span<Entry const> entries) = 0;
};

///
/// \brief Obtain the current thread ID.
/// \returns The thread ID
///
int thread_id();
///
/// \brief Format a log message given the level.
/// \param level The level of this log message
/// \param message The log message text
/// \returns Text formatted according to g_format
///
std::string format(Level level, std::string_view const message);

///
/// \brief Log an entry through a pipe.
/// \param pipe The pipe to log the text through
/// \param entry The log message entry
///
void log_to(Pipe pipe, Entry entry);
///
/// \brief Obtain the log buffer size.
/// \returns The log buffer size.
///
BufferSize const& buffer_size();
///
/// \brief Access the log buffer.
/// \param accessor Concrete sub-type of Accessor
///
void access_buffer(Accessor& accessor);

///
/// \brief RAII instance that initializes and empties the buffer.
///
class Instance : public Pinned {
  public:
	///
	/// \brief Initialize the log buffer.
	/// \param buffer_size Desired size of the log buffer
	///
	Instance(BufferSize const& buffer_size = {});
	~Instance();
};

///
/// \brief Format a log message.
/// \param level The log level
/// \param fmt Format string
/// \param args... Format arguments
/// \returns String formatted according to g_format
///
template <typename... Args>
std::string format(Level level, fmt::format_string<Args...> fmt, Args const&... args) {
	return format(level, fmt::vformat(fmt, fmt::make_format_args(args...)));
}

///
/// \brief Log an error message.
/// \param fmt Format string
/// \param args... Format arguments
///
template <typename... Args>
void error(fmt::format_string<Args...> fmt, Args const&... args) {
	log_to(Pipe::eStdErr, {format(Level::eError, fmt, args...), Level::eError});
}

///
/// \brief Log a warning message.
/// \param fmt Format string
/// \param args... Format arguments
///
template <typename... Args>
void warn(fmt::format_string<Args...> fmt, Args const&... args) {
	log_to(Pipe::eStdOut, {format(Level::eWarn, fmt, args...), Level::eWarn});
}

///
/// \brief Log an info message.
/// \param fmt Format string
/// \param args... Format arguments
///
template <typename... Args>
void info(fmt::format_string<Args...> fmt, Args const&... args) {
	log_to(Pipe::eStdOut, {format(Level::eInfo, fmt, args...), Level::eInfo});
}

///
/// \brief Log a debug message.
/// \param fmt Format string
/// \param args... Format arguments
///
template <typename... Args>
void debug(fmt::format_string<Args...> fmt, Args const&... args) {
	if constexpr (debug_v) { log_to(Pipe::eStdOut, {format(Level::eDebug, fmt, args...), Level::eDebug}); }
}
} // namespace levk::logger
