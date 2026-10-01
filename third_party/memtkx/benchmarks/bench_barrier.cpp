#include <chrono>
#include <cstddef>
#include <iostream>
#include <vector>

#include "dommemtk/DomMEMTk.hpp"

namespace {

using Clock = std::chrono::steady_clock;

double run_pipeline(std::size_t iterations) {
  DomMEMTk::Address slot_storage = 0;
  DomMEMTk::AddressRange source{DomMEMTk::as_address(&slot_storage),
                                DomMEMTk::as_address(&slot_storage) + 1};
  DomMEMTk::AddressRange target{0x1000, 0x2000};
  std::vector<DomMEMTk::Slot> buffer;
  DomMEMTk::BarrierDSL barrier;

  const auto start = Clock::now();
  for (std::size_t i = 0; i < iterations; ++i) {
    DomMEMTk::run_write_barrier(barrier, {&slot_storage, 0x1800}, source,
                                target, buffer);
  }
  const auto stop = Clock::now();
  return std::chrono::duration<double, std::nano>(stop - start).count() /
         static_cast<double>(iterations);
}

double run_raw_filter(std::size_t iterations) {
  DomMEMTk::Address slot_storage = 0;
  DomMEMTk::AddressRange source{DomMEMTk::as_address(&slot_storage),
                                DomMEMTk::as_address(&slot_storage) + 1};
  DomMEMTk::AddressRange target{0x1000, 0x2000};
  std::size_t hits = 0;

  const auto start = Clock::now();
  for (std::size_t i = 0; i < iterations; ++i) {
    const DomMEMTk::Address slot = DomMEMTk::as_address(&slot_storage);
    const DomMEMTk::Address object = 0x1800;
    if (source.contains(slot) && target.contains(object)) {
      ++hits;
    }
  }
  const auto stop = Clock::now();
  (void)hits;
  return std::chrono::duration<double, std::nano>(stop - start).count() /
         static_cast<double>(iterations);
}

}  // namespace

int main() {
  constexpr std::size_t iterations = 1000000;
  const double pipeline_ns = run_pipeline(iterations);
  const double raw_ns = run_raw_filter(iterations);

  std::cout << "barrier,ns_per_operation\n";
  std::cout << "pipeline," << pipeline_ns << '\n';
  std::cout << "raw-filter," << raw_ns << '\n';
  return 0;
}
