#pragma once
#include <levk/io/serializable.hpp>

namespace levk {
class Entity;
class Component;
struct AssetList;

struct Attachment : Serializable {
	virtual ~Attachment() = default;

	virtual void attach(Entity& out) = 0;
	virtual void add_assets(AssetList&) const {}
};
} // namespace levk
