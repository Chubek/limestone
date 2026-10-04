#include <cassert>
#include <cstddef>

#include "dommemtk/DomMEMTk.hpp"

int main() {
  using namespace DomMEMTk::literals;

  static_assert(1_KB == 1024);
  static_assert(1_MB == 1024 * 1024);
  static_assert(1_GB == 1024ULL * 1024ULL * 1024ULL);

  assert(DomMEMTk::is_power_of_two(8));
  assert(!DomMEMTk::is_power_of_two(6));

  assert(DomMEMTk::align_up(17, 8) == 24);
  assert(DomMEMTk::align_down(17, 8) == 16);

  DomMEMTk::AddressRange range{100, 200};
  assert(range.contains(100));
  assert(!range.contains(200));
  assert(range.size() == 100);

  assert(DomMEMTk::bytes_to_kib(2048) == 2);
  assert(DomMEMTk::mib_to_bytes(2) == 2 * 1024 * 1024);

  return 0;
}
