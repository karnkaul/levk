#pragma once

namespace levk {
///
/// \brief Alias for a single pointer.
///
/// This is purely a signal for readers (and syntactic sugar like auto x = Ptr<Foo>{}), as there's no difference to the compiler either way.
/// Using Ptr<T> for anything except "pointer to T" is undefined behavior (eg pointer arithmetic).
///
template <typename T>
using Ptr = T*;
} // namespace levk
