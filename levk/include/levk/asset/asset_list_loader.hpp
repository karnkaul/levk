#pragma once
#include <levk/asset/asset_list.hpp>
#include <levk/util/async_task.hpp>
#include <levk/util/not_null.hpp>

namespace levk {
class AssetProviders;
class ThreadPool;

class AssetListLoader : public AsyncTask<void> {
  public:
	AssetListLoader(NotNull<AssetProviders const*> asset_providers, AssetList list, Ptr<ThreadPool> thread_pool = {})
		: m_list(std::move(list)), m_asset_providers(asset_providers), m_thread_pool(thread_pool) {}

  protected:
	std::future<void> const& get_future() const override { return m_future; }
	void execute() override;

	AssetList m_list{};
	NotNull<AssetProviders const*> m_asset_providers;
	Ptr<ThreadPool> m_thread_pool{};

	std::future<void> m_future{};
};
} // namespace levk
