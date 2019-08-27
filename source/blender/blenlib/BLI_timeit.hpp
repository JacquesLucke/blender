#pragma once

/* This file contains utilities to make timing of
 * code segments easy.
 */

#include "BLI_sys_types.h"
#include <chrono>
#include <iostream>

namespace BLI {

namespace Timers {

using Clock = std::chrono::high_resolution_clock;
using TimePoint = Clock::time_point;
using Nanoseconds = std::chrono::nanoseconds;

inline void print_duration(Nanoseconds duration)
{
  if (duration.count() < 100000) {
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
  ScopedTimer(const char *name = "") : m_name(name)
  {
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

class ScopedTimerStatistics {
 private:
  TimePoint m_start;
  const char *m_name;
  Nanoseconds &m_shortest_duration;
  Nanoseconds &m_timings_sum;
  uint64_t &m_timings_done;

 public:
  ScopedTimerStatistics(const char *name,
                        Nanoseconds &shortest_duration,
                        Nanoseconds &timings_sum,
                        uint64_t &timings_done)
      : m_name(name),
        m_shortest_duration(shortest_duration),
        m_timings_sum(timings_sum),
        m_timings_done(timings_done)
  {
    m_start = Clock::now();
  }

  ~ScopedTimerStatistics()
  {
    TimePoint end = Clock::now();
    Nanoseconds duration = end - m_start;
    m_timings_sum += duration;
    m_timings_done++;

    if (duration < m_shortest_duration) {
      m_shortest_duration = duration;
    }

    Nanoseconds average_duration = m_timings_sum / m_timings_done;

    std::cout << "Timings stats for '" << m_name << "':\n";
    std::cout << "  Calls: " << m_timings_done << "\n";
    std::cout << "  Average: ";
    print_duration(average_duration);
    std::cout << "\n";
    std::cout << "  Shortest: ";
    print_duration(m_shortest_duration);
    std::cout << "\n";
    std::cout << "  Last: ";
    print_duration(duration);
    std::cout << "\n";
  }
};

class ScopedTimerPerElement {
 private:
  TimePoint m_start;
  const char *m_name;
  uint m_element_amount;

 public:
  ScopedTimerPerElement(const char *name, uint element_amount)
      : m_name(name), m_element_amount(element_amount)
  {
    m_start = Clock::now();
  }

  ~ScopedTimerPerElement()
  {
    TimePoint end = Clock::now();

    if (m_element_amount == 0) {
      return;
    }

    Nanoseconds duration = end - m_start;
    Nanoseconds duration_per_element = duration / m_element_amount;
    std::cout << "Timer '" << m_name << "' per element (" << m_element_amount << " elements): ";
    print_duration(duration_per_element);
    std::cout << '\n';
  }
};

}  // namespace Timers
}  // namespace BLI

#define SCOPED_TIMER(name) BLI::Timers::ScopedTimer t(name);

#define SCOPED_TIMER_STATS(name) \
  static uint64_t timings_done = 0; \
  static BLI::Timers::Nanoseconds shortest_duration = std::chrono::seconds(100); \
  static BLI::Timers::Nanoseconds timings_sum(0); \
  BLI::Timers::ScopedTimerStatistics t(name, shortest_duration, timings_sum, timings_done);

#define SCOPED_TIMER_ELEMENT(name, elements) BLI::Timers::ScopedTimerPerElement t(name, elements);
