#include "SatieMemory.hpp"

#include <algorithm>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include <dommemtk/space/bump_pointer.hpp>
#include <dommemtk/space/free_list.hpp>
#include <dommemtk/space/los_space.hpp>

namespace satie
{
struct MemoryResource::Impl
{
  struct Region
  {
    std::size_t bytes;
    std::size_t alignment;
    void *data;
    DomMEMTk::BumpPointerAllocator bump;
    DomMEMTk::FreeListAllocator free_list;

    Region (std::size_t size, std::size_t align)
        : bytes (size), alignment (align),
          data (std::pmr::new_delete_resource ()->allocate (size, align))
    {
      const auto start = DomMEMTk::as_address (static_cast<std::byte *> (data));
      bump.reset (start, start + bytes);
      try { free_list.reset (start, start + bytes); }
      catch (...)
        {
          std::pmr::new_delete_resource ()->deallocate (data, bytes, alignment);
          throw;
        }
    }
    ~Region ()
    {
      std::pmr::new_delete_resource ()->deallocate (data, bytes, alignment);
    }
  };
  MemoryLifetime lifetime;
  std::size_t block_bytes;
  std::size_t threshold;
  mutable std::mutex mutex;
  Statistics stats;
  std::vector<std::unique_ptr<Region>> pools;
  std::unordered_map<void *, std::unique_ptr<Region>> large;
};

MemoryResource::MemoryResource (MemoryLifetime lifetime, std::size_t block_bytes,
                                 std::size_t large_threshold)
    : impl_ (std::make_unique<Impl> ())
{
  if (block_bytes < alignof (std::max_align_t) || large_threshold == 0 ||
      large_threshold > block_bytes)
    throw std::invalid_argument ("invalid memory pool size or large-object threshold");
  impl_->lifetime = lifetime;
  impl_->block_bytes = block_bytes;
  impl_->threshold = large_threshold;
}
MemoryResource::~MemoryResource () = default;

void *MemoryResource::allocate_impl (std::size_t bytes, std::size_t alignment)
{
  if (!DomMEMTk::is_power_of_two (alignment))
    throw std::bad_alloc ();
  bytes = std::max<std::size_t> (bytes, 1);
  std::lock_guard lock (impl_->mutex);
  if (bytes > std::numeric_limits<std::size_t>::max () - alignment ||
      bytes > std::numeric_limits<std::size_t>::max () - impl_->stats.live_bytes)
    throw std::bad_alloc ();
  void *pointer = nullptr;
  if (bytes >= impl_->threshold || alignment > alignof (std::max_align_t))
    {
      const auto size = bytes + std::max (alignment, alignof (std::max_align_t));
      auto region = std::make_unique<Impl::Region> (
          size, std::max (alignment, alignof (std::max_align_t)));
      const auto start = DomMEMTk::as_address (static_cast<std::byte *> (region->data));
      DomMEMTk::LargeObjectSpace space ("satie-large", start, start + size, 1);
      auto allocated = space.allocate (bytes, alignment);
      if (!allocated.is_ok ())
        throw std::bad_alloc ();
      pointer = DomMEMTk::from_address<void> (allocated.unwrap ());
      impl_->large.emplace (pointer, std::move (region));
      impl_->stats.reserved_bytes += size;
      ++impl_->stats.large_allocations;
    }
  else
    {
      auto allocate_in = [&] (Impl::Region &region) {
        auto result = impl_->lifetime == MemoryLifetime::Transient
                          ? region.bump.allocate (bytes, alignment)
                          : region.free_list.allocate (bytes, alignment);
        return result.is_ok () ? DomMEMTk::from_address<void> (result.unwrap ()) : nullptr;
      };
      for (auto &pool : impl_->pools)
        if ((pointer = allocate_in (*pool)) != nullptr)
          break;
      if (!pointer)
        {
          auto region = std::make_unique<Impl::Region> (
              impl_->block_bytes, alignof (std::max_align_t));
          pointer = allocate_in (*region);
          if (!pointer)
            throw std::bad_alloc ();
          impl_->pools.push_back (std::move (region));
          impl_->stats.reserved_bytes += impl_->block_bytes;
        }
      ++impl_->stats.pooled_allocations;
    }
  ++impl_->stats.allocations;
  impl_->stats.live_bytes += bytes;
  impl_->stats.peak_bytes = std::max (impl_->stats.peak_bytes, impl_->stats.live_bytes);
  return pointer;
}

void MemoryResource::do_deallocate (void *pointer, std::size_t bytes, std::size_t)
{
  bytes = std::max<std::size_t> (bytes, 1);
  std::lock_guard lock (impl_->mutex);
  auto large = impl_->large.find (pointer);
  if (large != impl_->large.end ())
    {
      impl_->stats.reserved_bytes -= large->second->bytes;
      impl_->large.erase (large);
    }
  else if (impl_->lifetime == MemoryLifetime::Persistent)
    {
      const auto address = DomMEMTk::as_address (static_cast<std::byte *> (pointer));
      for (auto &pool : impl_->pools)
        if (pool->free_list.contains (address))
          {
            pool->free_list.free (address, bytes);
            break;
          }
    }
  impl_->stats.live_bytes -= bytes;
}

MemoryResource::Statistics MemoryResource::statistics () const
{
  std::lock_guard lock (impl_->mutex);
  return impl_->stats;
}
void MemoryResource::release ()
{
  std::lock_guard lock (impl_->mutex);
  impl_->pools.clear ();
  impl_->large.clear ();
  impl_->stats = {};
}
bool MemoryResource::do_is_equal (const std::pmr::memory_resource &other) const noexcept
{
  return this == &other;
}
} // namespace satie
