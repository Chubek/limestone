-- satie stdlib: array (array-table utilities, numeric keys only).
local Array = {}

Array.contains = function(items, value)
  local i = 1
  while i <= #items do
    if items[i] == value then return true end
    i = i + 1
  end
  return false
end

Array.index_of = function(items, value)
  local i = 1
  while i <= #items do
    if items[i] == value then return i end
    i = i + 1
  end
  return 0
end

Array.count_eq = function(items, value)
  local n = 0
  local i = 1
  while i <= #items do
    if items[i] == value then n = n + 1 end
    i = i + 1
  end
  return n
end

Array.remove_dup = function(items)
  local out = {}
  local i = 1
  while i <= #items do
    if Array.contains(out, items[i]) == false then
      out[#out + 1] = items[i]
    end
    i = i + 1
  end
  return out
end

Array.reversed = function(items)
  local out = {}
  local i = #items
  while i >= 1 do
    out[#out + 1] = items[i]
    i = i - 1
  end
  return out
end

Array.slice = function(items, from, to)
  local out = {}
  if to > #items then to = #items end
  local i = from
  while i <= to do
    out[#out + 1] = items[i]
    i = i + 1
  end
  return out
end

Array.sum = function(items)
  local total = 0
  local i = 1
  while i <= #items do
    total = total + items[i]
    i = i + 1
  end
  return total
end

return Array
