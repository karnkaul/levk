#pragma once

namespace levk {
class Window;

struct Surface {
	struct Source {
		virtual ~Source() = default;
	};

	virtual ~Surface() = default;
};
} // namespace levk
