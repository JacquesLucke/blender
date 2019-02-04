#pragma once

#include <chrono>
#include <string>

namespace CholUp {

struct Timer
{
    typedef std::chrono::system_clock clock;
    typedef std::chrono::time_point<clock> timepoint;

    timepoint start;
    std::string name;
    clock::duration sum;
    bool isPaused = false;

    explicit Timer(const std::string& str = "Timer");

    void pause();
    void resume();
    clock::duration elapsed() const;

    void reset();
    int seconds() const;
    int milliseconds() const;
    int microseconds() const;
    int minutes() const;
    int hours() const;
    void printTime(const std::string& str = "") const;
};

} /* namespace CholUp */