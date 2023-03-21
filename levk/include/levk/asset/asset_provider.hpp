#pragma once
#include <djson/json.hpp>
#include <levk/uri.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/not_null.hpp>
#include <levk/util/ptr.hpp>
#include <levk/vfs/data_source.hpp>
#include <levk/vfs/uri_monitor.hpp>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace levk {
template <typename Type>
class AssetProvider {
  public:
	AssetProvider(NotNull<DataSource const*> data_source, NotNull<UriMonitor*> uri_monitor) : m_storage(std::make_unique<Storage>(data_source, uri_monitor)) {}

	Ptr<Type const> find(Uri<Type> const& uri) const {
		if (!uri) { return {}; }
		auto lock = std::scoped_lock{m_storage->mutex};
		if (auto it = m_storage->map.find(uri); it != m_storage->map.end()) { return &*it->second.asset; }
		return {};
	}

	Ptr<Type> find(Uri<Type> const& uri) {
		if (!uri) { return {}; }
		auto lock = std::scoped_lock{m_storage->mutex};
		if (auto it = m_storage->map.find(uri); it != m_storage->map.end()) { return &*it->second.asset; }
		return {};
	}

	Type const& get(Uri<Type> const& uri, Type const& fallback) const {
		if (auto* ret = find(uri)) { return *ret; }
		return fallback;
	}

	Ptr<Type> load(Uri<Type> const& uri) {
		if (!uri) { return {}; }
		if (auto ret = find(uri)) { return ret; }
		return do_load(uri);
	}

	Ptr<Type> add(Uri<Type> const& uri, Type t) {
		if (!uri) { return {}; }
		auto lock = std::scoped_lock{m_storage->mutex};
		auto [it, _] = m_storage->map.insert_or_assign(uri, Entry{std::move(t)});
		return &*it->second.asset;
	}

	void remove(Uri<Type> const& uri) {
		auto lock = std::scoped_lock{m_storage->mutex};
		m_storage->map.erase(uri);
	}

	void reload_out_of_date() {
		auto lock = std::unique_lock{m_storage->mutex};
		auto out_of_date = std::move(m_storage->out_of_date);
		lock.unlock();
		for (auto& uri : out_of_date) { do_load(uri); }
	}

	DataSource const& data_source() const { return *m_storage->data_source; }
	UriMonitor& uri_monitor() const { return *m_storage->uri_monitor; }

	ByteArray read_bytes(Uri<> const& uri) const { return data_source().read(uri); }

	std::string read_text(Uri<> const& uri) const {
		auto const bytes = read_bytes(uri);
		if (!bytes) { return {}; }
		return std::string{data_source().as_text(bytes.span())};
	}

	template <typename Func>
	void for_each(Func func) const {
		auto lock = std::scoped_lock{m_storage->mutex};
		for (auto& [uri, entry] : m_storage->map) { func(uri, *entry.asset); }
	}

  protected:
	struct Payload {
		std::optional<Type> asset{};
		std::vector<Uri<>> dependencies{};
	};

	virtual Payload load_payload(Uri<Type> const& uri) const = 0;

  private:
	struct Entry {
		std::optional<Type> asset{};
		std::vector<UriMonitor::OnModified::Listener> listeners{};
	};

	Ptr<Type> do_load(Uri<> const& uri) {
		if (auto payload = load_payload(uri); payload.asset) {
			auto listeners = std::vector<UriMonitor::OnModified::Listener>{};
			for (auto const& dependency : payload.dependencies) {
				listeners.push_back(m_storage->uri_monitor->on_modified(dependency).connect([this, uri](Uri<> const&) { m_storage->out_of_date.insert(uri); }));
			}
			auto lock = std::scoped_lock{m_storage->mutex};
			auto [it, _] = m_storage->map.insert_or_assign(uri, Entry{std::move(payload.asset), std::move(listeners)});
			return &*it->second.asset;
		}
		return {};
	}

	struct Storage {
		std::unordered_map<Uri<>, Entry, Uri<>::Hasher> map{};
		std::unordered_set<Uri<>, Uri<>::Hasher> out_of_date{};
		std::mutex mutex{};
		NotNull<DataSource const*> data_source;
		NotNull<UriMonitor*> uri_monitor;

		Storage(NotNull<DataSource const*> data_source, NotNull<UriMonitor*> uri_monitor) : data_source(data_source), uri_monitor(uri_monitor) {}
	};

	std::unique_ptr<Storage> m_storage{};
};

class RenderDevice;

template <typename Type>
class GraphicsAssetProvider : public AssetProvider<Type> {
  public:
	GraphicsAssetProvider(NotNull<RenderDevice*> render_device, NotNull<DataSource const*> data_source, NotNull<UriMonitor*> uri_monitor)
		: AssetProvider<Type>(data_source, uri_monitor), m_render_device(render_device) {}

	RenderDevice& render_device() const { return *m_render_device; }

  private:
	NotNull<RenderDevice*> m_render_device;
};
} // namespace levk
