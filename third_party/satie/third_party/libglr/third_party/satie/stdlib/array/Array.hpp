#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

namespace satie::stdlib::array
{

template <typename T> bool contains (const std::vector<T> &items, const T &value)
{
  return std::find (items.begin (), items.end (), value) != items.end ();
}

template <typename T>
std::size_t count_eq (const std::vector<T> &items, const T &value)
{
  return static_cast<std::size_t> (std::count (items.begin (), items.end (), value));
}

/// Index of the first occurrence, or `items.size()` when absent.
template <typename T>
std::size_t index_of (const std::vector<T> &items, const T &value)
{
  auto it = std::find (items.begin (), items.end (), value);
  return static_cast<std::size_t> (std::distance (items.begin (), it));
}

template <typename T> std::vector<T> remove_duplicates (const std::vector<T> &items)
{
  std::vector<T> out;
  for (const T &item : items)
    if (!contains (out, item))
      out.push_back (item);
  return out;
}

template <typename T> std::vector<T> reversed (const std::vector<T> &items)
{
  return std::vector<T> (items.rbegin (), items.rend ());
}

/// Half-open slice `[from, to)` clamped to the input range.
template <typename T>
std::vector<T> slice (const std::vector<T> &items, std::size_t from, std::size_t to)
{
  from = std::min (from, items.size ());
  to = std::min (to, items.size ());
  if (to < from)
    return {};
  return std::vector<T> (items.begin () + static_cast<std::ptrdiff_t> (from),
                         items.begin () + static_cast<std::ptrdiff_t> (to));
}

} // namespace satie::stdlib::array
