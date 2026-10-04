#include <cassert>
#include <cstdint>

#include "dommemtk/DomMEMTk.hpp"

namespace {

enum class GCState : std::uint8_t {
  Unmarked = 0,
  Marked = 1,
  Forwarded = 2,
  Pinned = 3,
};

struct TraceDispatcher
    : dsl::DSL<TraceDispatcher, dsl::PatternMatch> {
  static constexpr auto trace_object = dsl::match(
      dsl::when<GCState::Unmarked>([](GCState, DomMEMTk::Address) {
        return GCState::Marked;
      }),
      dsl::when<GCState::Marked>([](GCState, DomMEMTk::Address) {
        return GCState::Marked;
      }),
      dsl::when<GCState::Forwarded>([](GCState, DomMEMTk::Address) {
        return GCState::Forwarded;
      }),
      dsl::otherwise([](GCState, DomMEMTk::Address) {
        return GCState::Pinned;
      }));
};

}  // namespace

int main() {
  assert(TraceDispatcher::trace_object(GCState::Unmarked, 0) ==
         GCState::Marked);
  assert(TraceDispatcher::trace_object(GCState::Forwarded, 0) ==
         GCState::Forwarded);
  assert(TraceDispatcher::trace_object(GCState::Pinned, 0) ==
         GCState::Pinned);
  return 0;
}
