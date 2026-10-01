#pragma once

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace satie::stdlib::list
{

template <typename T, typename Fn>
auto transform (const std::vector<T> &items, Fn &&fn)
{
  using Out = std::decay_t<decltype (fn (std::declval<const T &> ()))>;
  std::vector<Out> out;
  out.reserve (items.size ());
  for (const T &item : items)
    out.push_back (fn (item));
  return out;
}

template <typename T, typename Pred>
std::vector<T> filter (const std::vector<T> &items, Pred &&pred)
{
  std::vector<T> out;
  for (const T &item : items)
    if (pred (item))
      out.push_back (item);
  return out;
}

template <typename T, typename Acc, typename Fn>
Acc fold (const std::vector<T> &items, Acc init, Fn &&fn)
{
  for (const T &item : items)
    init = fn (std::move (init), item);
  return init;
}

template <typename T, typename Pred> bool any_of (const std::vector<T> &items, Pred &&pred)
{
  for (const T &item : items)
    if (pred (item))
      return true;
  return false;
}

template <typename T, typename Pred> bool all_of (const std::vector<T> &items, Pred &&pred)
{
  for (const T &item : items)
    if (!pred (item))
      return false;
  return true;
}

template <typename A, typename B>
std::vector<std::pair<A, B>> zip (const std::vector<A> &left, const std::vector<B> &right)
{
  std::vector<std::pair<A, B>> out;
  const std::size_t count = std::min (left.size (), right.size ());
  for (std::size_t i = 0; i < count; ++i)
    out.emplace_back (left[i], right[i]);
  return out;
}

inline std::vector<int> range (int from, int to, int step = 1)
{
  if (step == 0)
    throw std::invalid_argument ("list::range step must be non-zero");
  std::vector<int> out;
  if (step > 0)
    for (int i = from; i < to; i += step)
      out.push_back (i);
  else
    for (int i = from; i > to; i += step)
      out.push_back (i);
  return out;
}

} // namespace satie::stdlib::list
