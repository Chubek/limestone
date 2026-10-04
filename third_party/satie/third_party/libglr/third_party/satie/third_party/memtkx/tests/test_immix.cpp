#include <cassert>
#include <cstddef>

#include "dommemtk/DomMEMTk.hpp"

int main() {
  constexpr std::size_t requested = DomMEMTk::kImmixBlockBytes + 1;
  DomMEMTk::ImmixSpace space(requested);

  auto head = space.allocate(DomMEMTk::kImmixBlockBytes, DomMEMTk::kImmixLineBytes);
  auto tail = space.allocate(1, DomMEMTk::kImmixLineBytes);
  assert(head.is_ok());
  assert(tail.is_ok());
  assert(space.block_count() >= 2);
  assert(space.total_bytes() >= requested);

  const DomMEMTk::AddressRange base_range{head.unwrap(), head.unwrap() + 1};
  assert(space.mark(base_range.start, 1));
  assert(space.marked_bytes() >= DomMEMTk::kImmixLineBytes);
  space.reset_blocks();
  assert(space.used_bytes() == 0);

  return 0;
}
