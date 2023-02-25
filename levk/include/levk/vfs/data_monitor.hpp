#pragma once
#include <levk/util/signal.hpp>
#include <levk/vfs/data_sink.hpp>

namespace levk {
struct DataMonitor : DataSink {
	using OnModified = Signal<Uri<> const&>;

	virtual OnModified::Handle connect(Uri<> uri, OnModified::Callback on_modified) = 0;
	virtual std::uint32_t reload_modified(int max_reloads) = 0;

	template <typename T, typename FuncT>
	OnModified::Handle connect(Uri<> uri, T* self, FuncT member_func) {
		return connect(std::move(uri), [self, member_func](auto const& x) { std::invoke(member_func, self, x); });
	}
};
} // namespace levk
