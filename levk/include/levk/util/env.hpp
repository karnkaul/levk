#pragma once
#include <span>
#include <string>
#include <vector>

namespace levk::env {
using MatchList = std::span<std::string_view const>;

struct DirEntries {
	std::vector<std::string> dirs{};
	std::vector<std::string> files{};
};

struct GetDirEntries {
	MatchList extensions{};
	bool sort{true};
	bool show_hidden{false};

	bool operator()(DirEntries& out_entries, char const* path) const;
};

///
/// \brief Obtain the filename of a file drop path.
/// \param path Path to extract filename from
/// \returns Filename
///
std::string to_filename(std::string_view path);

///
/// \brief Obtain the user documents path.
/// \returns Documents path in generic form
///
std::string documents_path();
///
/// \brief Obtain the user documents path.
/// \returns Downloads path in generic form
///
std::string downloads_path();
} // namespace levk::env
