-- satie stdlib: list (higher-order array functions).
local List = {}

List.map = function(items, fn)
  local out = {}
  local i = 1
  while i <= #items do
    out[#out + 1] = fn(items[i])
    i = i + 1
  end
  return out
end

List.filter = function(items, pred)
  local out = {}
  local i = 1
  while i <= #items do
    if pred(items[i]) then
      out[#out + 1] = items[i]
    end
    i = i + 1
  end
  return out
end

List.fold = function(items, init, fn)
  local acc = init
  local i = 1
  while i <= #items do
    acc = fn(acc, items[i])
    i = i + 1
  end
  return acc
end

List.any = function(items, pred)
  local i = 1
  while i <= #items do
    if pred(items[i]) then return true end
    i = i + 1
  end
  return false
end

List.all = function(items, pred)
  local i = 1
  while i <= #items do
    if pred(items[i]) == false then return false end
    i = i + 1
  end
  return true
end

List.range = function(from, to)
  local out = {}
  local i = from
  while i < to do
    out[#out + 1] = i
    i = i + 1
  end
  return out
end

return List
