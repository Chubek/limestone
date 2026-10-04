#include "catch_shim.hpp"
#include "SatieMemory.hpp"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <vector>

using namespace satie;

TEST_CASE ("PMR vector growth uses the custom resource for both lifetimes")
{
  for (MemoryLifetime lifetime : { MemoryLifetime::Persistent, MemoryLifetime::Transient })
    {
      MemoryResource memory (lifetime, 256, 128);
      {
        std::pmr::vector<int> stack (&memory);
        for (int value = 0; value < 200; ++value)
          stack.push_back (value);
        for (std::size_t index = 0; index < stack.size (); ++index)
          REQUIRE (stack[index] == static_cast<int> (index));

        const auto stats = memory.statistics ();
        REQUIRE (stats.allocations > 1);
        REQUIRE (stats.pooled_allocations > 0);
        REQUIRE (stats.large_allocations > 0);
        REQUIRE (stats.live_bytes == stack.capacity () * sizeof (int));
        REQUIRE (stats.peak_bytes >= stats.live_bytes);
        REQUIRE (stats.reserved_bytes >= stats.live_bytes);
      }
      REQUIRE (memory.statistics ().live_bytes == 0);
      memory.release ();
      REQUIRE (memory.statistics ().allocations == 0);
      REQUIRE (memory.statistics ().reserved_bytes == 0);
    }
}

TEST_CASE ("PMR range construction accounts for empty, pooled, and large ranges")
{
  for (std::size_t count : { std::size_t{ 0 }, std::size_t{ 1 }, std::size_t{ 8 },
                            std::size_t{ 512 } })
    {
      std::vector<std::size_t> indices (count);
      std::iota (indices.begin (), indices.end (), std::size_t{ 0 });
      MemoryResource memory (MemoryLifetime::Transient, 1024, 256);
      {
        std::pmr::vector<std::size_t> order (indices.begin (), indices.end (), &memory);
        REQUIRE (order.size () == indices.size ());
        REQUIRE (std::equal (order.begin (), order.end (), indices.begin ()));
        const auto stats = memory.statistics ();
        REQUIRE (stats.allocations == (count == 0 ? 0 : 1));
        REQUIRE (stats.live_bytes == order.capacity () * sizeof (std::size_t));
        if (count != 0)
          {
            const bool large = count * sizeof (std::size_t) >= 256;
            REQUIRE (stats.large_allocations == (large ? 1 : 0));
            REQUIRE (stats.pooled_allocations == (large ? 0 : 1));
          }
      }
      REQUIRE (memory.statistics ().live_bytes == 0);
    }
}

TEST_CASE ("PMR allocation preserves over-alignment and values")
{
  struct alignas (64) Aligned
  {
    std::uint64_t value;
  };
  for (MemoryLifetime lifetime : { MemoryLifetime::Persistent, MemoryLifetime::Transient })
    {
      MemoryResource memory (lifetime);
      {
        std::pmr::vector<Aligned> values (&memory);
        for (std::uint64_t value = 0; value < 16; ++value)
          values.push_back ({ value });
        REQUIRE (reinterpret_cast<std::uintptr_t> (values.data ()) % alignof (Aligned) == 0);
        for (std::size_t index = 0; index < values.size (); ++index)
          REQUIRE (values[index].value == index);
        REQUIRE (memory.statistics ().pooled_allocations == 0);
        REQUIRE (memory.statistics ().large_allocations > 0);
      }
      REQUIRE (memory.statistics ().live_bytes == 0);
      REQUIRE (memory.statistics ().reserved_bytes == 0);
    }
}

SATIE_RUN_MAIN
