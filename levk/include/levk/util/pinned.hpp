#pragma once

namespace levk {
///
/// \brief Base class for non-moveable, non-copiable types.
///
class Pinned {
  public:
	Pinned() = default;
	Pinned(Pinned&&) = delete;
	Pinned& operator=(Pinned&&) = delete;
	Pinned(Pinned const&) = delete;
	Pinned& operator=(Pinned const&) = delete;
};
} // namespace levk
