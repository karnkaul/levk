#pragma once
#include <levk/asset/asset_list.hpp>
#include <levk/util/async_task.hpp>
#include <levk/util/not_null.hpp>

namespace levk {
class AssetProviders;

class AssetListLoader : public AsyncTask<void> {
  public:
	AssetListLoader(NotNull<AssetProviders const*> asset_providers, AssetList list) : m_list(std::move(list)), m_asset_providers(asset_providers) {}

  protected:
	ScopedFuture<void> const& get_future() const override { return m_future; }
	void execute(ThreadPool& thread_pool) override;

	AssetList m_list{};
	NotNull<AssetProviders const*> m_asset_providers;

	ScopedFuture<void> m_future{};
};
} // namespace levk
