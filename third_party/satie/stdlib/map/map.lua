-- satie stdlib: map (association lists: arrays of {key, value} pairs).
local Map = {}

Map.lookup = function(assoc, key)
  local i = 1
  while i <= #assoc do
    if assoc[i][1] == key then return assoc[i][2] end
    i = i + 1
  end
  return nil
end

Map.keys = function(assoc)
  local out = {}
  local i = 1
  while i <= #assoc do
    out[#out + 1] = assoc[i][1]
    i = i + 1
  end
  return out
end

Map.values = function(assoc)
  local out = {}
  local i = 1
  while i <= #assoc do
    out[#out + 1] = assoc[i][2]
    i = i + 1
  end
  return out
end

Map.upsert = function(assoc, key, value)
  local i = 1
  while i <= #assoc do
    if assoc[i][1] == key then
      assoc[i][2] = value
      return assoc
    end
    i = i + 1
  end
  assoc[#assoc + 1] = {key, value}
  return assoc
end

Map.remove = function(assoc, key)
  local out = {}
  local found = false
  local i = 1
  while i <= #assoc do
    if assoc[i][1] == key then
      found = true
    else
      out[#out + 1] = assoc[i]
    end
    i = i + 1
  end
  return out
end

return Map
