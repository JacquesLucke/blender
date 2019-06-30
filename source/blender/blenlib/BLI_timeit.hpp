#pragma once

/* This file contains utilities to make timing of
 * code segments easy.
 */

#include <chrono>
#include <iostream>

namespace BLI {

namespace Timers {

using Clock = std::chrono::high_resolution_clock;
using TimePoint = Clock::time_point;
using Nanoseconds = std::chrono::nanoseconds;

inline void print_duration(Nanoseconds duration)
{
  if (duration.count() < 10000) {
    std::cout << duration.count() << " ns";
  }
  else {
    std::cout << duration.count() / 1000000.0 << " ms";
  }
}

class ScopedTimer {
 private:
  const char *m_name;
  TimePoint m_start;

 public:
  ScopedTimer(const char *name = "")
  {
    m_name = name;
    m_start = Clock::now();
  }

  ~ScopedTimer()
  {
    TimePoint end = Clock::now();
    Nanoseconds duration = end - m_start;
    std::cout << "Timer '" << m_name << "' took ";
    print_duration(duration);
    std::cout << "\n";
  }
};

}  // namespace Timers

};  // namespace BLI

#define SCOPED_TIMER(name) BLI::Timers::ScopedTimer t(name);
