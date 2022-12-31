#pragma once
#include <cstddef>
#include <memory>
#include <span>
#include <string>

namespace levk {
struct Reader {
	enum Flag : std::uint8_t {
		eNone = 0,
		eReload = 1 << 0,
		eComputeHash = 1 << 1,
		eSilent = 1 << 2,
	};

	struct Data {
		std::span<std::byte const> bytes{};
		std::size_t hash{};

		explicit operator bool() const { return !bytes.empty(); }
	};

	static std::size_t compute_hash(std::span<std::byte const> bytes);

	virtual ~Reader() = default;

	virtual Data load(std::string const& uri, std::uint8_t flags = {}) = 0;
};

class FileReader : public Reader {
  public:
	FileReader();
	FileReader(FileReader&&) noexcept;
	FileReader& operator=(FileReader&&) noexcept;
	~FileReader();

	bool mount(std::string_view directory);
	std::string absolute_path_for(std::string_view const uri) const;

	Data load(std::string const& uri, std::uint8_t flags = {}) override;
	std::uint32_t reload_out_of_date(bool silent = false);

  private:
	Data reload(std::string const& uri, std::uint8_t flags);

	struct Impl;
	std::unique_ptr<Impl> m_impl{};
};
} // namespace levk
