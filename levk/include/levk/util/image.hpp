#pragma once
#include <levk/util/unique.hpp>
#include <glm/vec2.hpp>
#include <span>
#include <string>

namespace levk {
constexpr std::byte operator""_B(unsigned long long l) { return static_cast<std::byte>(l); }

///
/// \brief Storage for uncompressed RGBA image data (as bytes)
///
class Image {
  public:
	///
	/// \brief View into an image.
	///
	/// Must not outlive the image.
	///
	struct View {
		std::span<std::byte const> bytes{};
		glm::uvec2 extent{};
	};

	Image() = default;

	///
	/// \brief Construct an instance and decompress image data (PNG, JPG, TGA, etc).
	/// \param compressed Image data (compressed)
	/// \param name Name of the image (optional)
	///
	explicit Image(std::span<std::byte const> compressed, std::string name = {});

	///
	/// \brief Obtain a view into the stored image.
	/// \returns View into the image
	///
	View view() const;
	///
	/// \brief Obtain the name.
	/// \returns The name
	///
	std::string_view name() const { return m_name; }

	operator View() const { return view(); }

	///
	/// \brief Check if any image data is stored in this instance.
	/// \returns true If extent is non-zero and image data is present
	///
	explicit operator bool() const;

  private:
	struct Storage {
		std::size_t size{};
		std::byte const* data{};

		std::span<std::byte const> bytes() const { return {data, size}; }

		struct Deleter {
			void operator()(Storage const& storage) const;
		};
	};

	std::string m_name{};
	Unique<Storage, Storage::Deleter> m_storage{};
	glm::uvec2 m_extent{};
};

template <std::uint32_t Width, std::uint32_t Height, std::uint32_t Channels = 4>
struct FixedBitmap {
	std::byte bytes[Width * Height * Channels]{};

	constexpr Image::View view() const { return {.bytes = bytes, .extent = {Width, Height}}; }
	constexpr operator Image::View() const { return view(); }
};
} // namespace levk
