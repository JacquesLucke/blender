/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <functional>
#include <type_traits>

namespace blender {

template<typename F, typename Ret, typename... Args>
static constexpr bool is_callable_v = std::is_convertible_v<F, std::function<Ret(Args...)>>;

}
