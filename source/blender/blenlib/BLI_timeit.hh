/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#include <chrono>
#include <iostream>
#include <string>

#include "BLI_sys_types.h"

namespace blender::timeit {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using Nanoseconds = std::chrono::nanoseconds;

void print_duration(Nanoseconds duration);

class ScopedTimer {
 private:
  bool is_active_;
  std::string name_;
  TimePoint start_;

 public:
  ScopedTimer(std::string name, bool is_active = true)
      : is_active_(is_active), name_(std::move(name))
  {
    if (is_active) {
      start_ = Clock::now();
    }
  }

  ~ScopedTimer()
  {
    if (!is_active_) {
      return;
    }
    const TimePoint end = Clock::now();
    const Nanoseconds duration = end - start_;

    std::cout << "Timer '" << name_ << "' took ";
    print_duration(duration);
    std::cout << '\n';
  }
};

}  // namespace blender::timeit

#define SCOPED_TIMER(name) blender::timeit::ScopedTimer scoped_timer(name)
#define SCOPED_TIMER_CONDITION(name, is_active) \
  blender::timeit::ScopedTimer scoped_timer(name, is_active)
