#pragma once

#include <cstddef>
#include <memory>
#include <memory_resource>

namespace satie
{

enum class MemoryLifetime { Transient, Persistent };

/// memtkx-backed PMR resource. Transient storage uses bump allocation;
/// persistent storage uses reusable free lists. Large requests have separate
/// regions so they cannot fragment the small-object pools.
/// Destroy all clients before release(). Resources themselves are not copyable.
class MemoryResource final : public std::pmr::memory_resource
{
public:
  struct Statistics
  {
    std::size_t allocations = 0;
    std::size_t live_bytes = 0;
    std::size_t peak_bytes = 0;
    std::size_t reserved_bytes = 0;
    std::size_t pooled_allocations = 0;
    std::size_t large_allocations = 0;
  };

  explicit MemoryResource (MemoryLifetime lifetime = MemoryLifetime::Persistent,
                           std::size_t block_bytes = 64 * 1024,
                           std::size_t large_threshold = 4096);
  ~MemoryResource () override;
  MemoryResource (const MemoryResource &) = delete;
  MemoryResource &operator= (const MemoryResource &) = delete;
  Statistics statistics () const;
  /// Reclaims all regions and resets statistics; invalidates every allocation.
  void release ();

private:
  // Keep the allocation hook visible so PMR calls can devirtualize to this
  // resource instead of speculating on an unrelated standard-library resource.
  void *do_allocate (std::size_t bytes, std::size_t alignment) override
  {
    return allocate_impl (bytes, alignment);
  }
  void *allocate_impl (std::size_t bytes, std::size_t alignment);
  void do_deallocate (void *pointer, std::size_t bytes, std::size_t alignment) override;
  bool do_is_equal (const std::pmr::memory_resource &other) const noexcept override;
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace satie
