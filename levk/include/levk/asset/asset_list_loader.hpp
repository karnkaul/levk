#pragma once
#include <levk/asset/asset_list.hpp>
#include <levk/util/async_task.hpp>
#include <levk/util/not_null.hpp>

namespace levk {
class AssetProviders;

class AssetListLoader : public AsyncTask<void> {
  public:
	AssetListLoader(NotNull<AssetProviders const*> asset_providers, AssetList list) : m_asset_providers(asset_providers), m_list(std::move(list)) {}

  protected:
	void execute() override;

	NotNull<AssetProviders const*> m_asset_providers;
	AssetList m_list{};
};
} // namespace levk
