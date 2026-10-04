#pragma once

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace satie::stdlib::map
{

/// Association list (array of pairs) helpers: the Lua-facing map shape,
/// where iteration order is explicit.
template <typename K, typename V>
std::optional<V> assoc_lookup (const std::vector<std::pair<K, V>> &assoc, const K &key)
{
  for (const auto &[k, v] : assoc)
    if (k == key)
      return v;
  return std::nullopt;
}

template <typename K, typename V>
std::vector<K> assoc_keys (const std::vector<std::pair<K, V>> &assoc)
{
  std::vector<K> out;
  for (const auto &[k, v] : assoc)
    {
      (void)v;
      out.push_back (k);
    }
  return out;
}

template <typename K, typename V>
std::vector<V> assoc_values (const std::vector<std::pair<K, V>> &assoc)
{
  std::vector<V> out;
  for (const auto &[k, v] : assoc)
    {
      (void)k;
      out.push_back (v);
    }
  return out;
}

template <typename K, typename V>
void assoc_upsert (std::vector<std::pair<K, V>> &assoc, K key, V value)
{
  for (auto &[k, v] : assoc)
    if (k == key)
      {
        v = std::move (value);
        return;
      }
  assoc.emplace_back (std::move (key), std::move (value));
}

template <typename K, typename V>
bool assoc_remove (std::vector<std::pair<K, V>> &assoc, const K &key)
{
  for (auto it = assoc.begin (); it != assoc.end (); ++it)
    if (it->first == key)
      {
        assoc.erase (it);
        return true;
      }
  return false;
}

/// `std::map` conveniences.
template <typename K, typename V>
V map_get_or (const std::map<K, V> &mapping, const K &key, V fallback)
{
  auto it = mapping.find (key);
  return it == mapping.end () ? std::move (fallback) : it->second;
}

template <typename K, typename V>
std::map<K, V> map_merged (std::map<K, V> left, const std::map<K, V> &right)
{
  for (const auto &[k, v] : right)
    left[k] = v;
  return left;
}

} // namespace satie::stdlib::map
