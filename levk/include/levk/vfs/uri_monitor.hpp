#pragma once
#include <levk/uri.hpp>
#include <levk/util/signal.hpp>
#include <chrono>
#include <mutex>
#include <unordered_map>

namespace levk {
class UriMonitor {
  public:
	using Timestamp = std::chrono::milliseconds;
	using OnModified = Signal<Uri<> const&>;

	explicit UriMonitor(std::string mount_point);

	std::string_view mount_point() const { return m_mount_point; }

	Timestamp last_modified(Uri<> const& uri) const;
	OnModified& on_modified(Uri<> const& uri);
	void dispatch_modified();

  private:
	struct Monitor {
		OnModified on_modified{};
		Timestamp timestamp{};
	};

	std::unordered_map<Uri<>, Monitor, Uri<>::Hasher> m_monitors{};
	std::string m_mount_point{};
	mutable std::mutex m_mutex{};
};
} // namespace levk
