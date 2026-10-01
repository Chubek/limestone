#include <chrono>
#include <cassert>

#include "dommemtk/DomMEMTk.hpp"

int main() {
  DomMEMTk::CollectorCoordinator coordinator(2);

  auto timed_out = coordinator.request_stop_the_world(std::chrono::milliseconds(10));
  assert(timed_out.is_err());
  assert(timed_out.error() == DomMEMTk::GCError::SafepointTimeout);

  DomMEMTk::ThreadRecord record;
  record.id = 1;
  record.name = "mutator";
  assert(coordinator.register_mutator(record));
  assert(coordinator.registered_mutators() == 1);
  assert(coordinator.status(1) == DomMEMTk::MutatorStatus::Running);
  assert(coordinator.unregister_mutator(1));

  DomMEMTk::PhaseState state;
  auto phase1 = DomMEMTk::make_phase(
      "warning", [](DomMEMTk::PhaseState& phase_state) {
        return DomMEMTk::GcResult<int>::from_err(DomMEMTk::GCError::InvalidTrace);
      },
      /*stop_on_error=*/false);
  auto phase2 = DomMEMTk::make_phase(
      "continue", [](DomMEMTk::PhaseState& phase_state) {
        phase_state.note_completion();
        return DomMEMTk::GcResult<int>::from_ok(1);
      });

  auto pipeline = phase1 | phase2;
  auto result = pipeline.run(state);
  assert(result.is_ok());
  assert(state.completed_steps == 1);

  return 0;
}
