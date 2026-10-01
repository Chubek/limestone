#include <cassert>
#include <chrono>
#include <thread>
#include <vector>

#include "dommemtk/DomMEMTk.hpp"

int main() {
  DomMEMTk::WorkQueue queue;
  queue.push({DomMEMTk::WorkKind::RootScan, 10, 0});
  queue.push({DomMEMTk::WorkKind::Mark, 20, 0});
  std::size_t processed = queue.drain(
      [](const DomMEMTk::WorkPacket& packet) {
        assert(packet.object != 0);
      });
  assert(processed == 2);
  assert(queue.empty());

  DomMEMTk::PhaseState state;
  auto pipeline =
      DomMEMTk::make_phase(
          "stop", [](DomMEMTk::PhaseState&) {
            return DomMEMTk::GcResult<int>::from_ok(1);
          }) |
      DomMEMTk::make_phase(
          "roots", [](DomMEMTk::PhaseState& phase) {
            phase.note_completion(2);
            return DomMEMTk::GcResult<int>::from_ok(2);
          }) |
      DomMEMTk::make_phase(
          "closure", [](DomMEMTk::PhaseState&) {
            return DomMEMTk::GcResult<int>::from_ok(3);
          });

  auto result = pipeline.run(state);
  assert(result.is_ok());
  assert(state.completed_steps == 2);
  assert(pipeline.size() == 3);

  DomMEMTk::CollectorCoordinator coordinator(1);
  DomMEMTk::ThreadRecord record;
  record.id = 1;
  record.name = "mutator";
  assert(coordinator.register_mutator(record));
  assert(coordinator.registered_mutators() == 1);

  DomMEMTk::MutatorContext mutator(coordinator, 1);
  DomMEMTk::CollectorContext collector(coordinator);

  std::thread mutator_thread([&] {
    auto polled = mutator.poll();
    assert(polled.is_ok());
  });

  auto stopped = collector.stop_the_world(std::chrono::seconds(3));
  assert(stopped.is_ok());
  collector.release_mutators();
  mutator_thread.join();

  assert(coordinator.unregister_mutator(1));
  assert(coordinator.registered_mutators() == 0);

  return 0;
}
