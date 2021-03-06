#include <iostream>
#include <chrono>

#include "timing/timing.hpp"

#define HRC std::chrono::high_resolution_clock

namespace Timer {
  namespace {
    HRC::time_point start = 
      HRC::now();
    HRC::time_point lastDelta = HRC::now();
  }

  double GetGlobalTime() {
    return std::chrono::duration<double, std::milli>(HRC::now() - start).count();
  }

  double GetDelta() {
    HRC::time_point newDelta = HRC::now();
    std::chrono::duration<double, std::milli> dur(newDelta - lastDelta);
    lastDelta = newDelta;
    return dur.count();
  }

  double PollDelta() {
    HRC::time_point newDelta = HRC::now();
    std::chrono::duration<double, std::milli> dur(newDelta - lastDelta);
    return dur.count();
  }
};
