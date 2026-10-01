-- satie stdlib: json (encoder side only).
-- The dialect cannot dispatch on value types or iterate object keys, so
-- callers use per-type functions; objects encode from parallel key/value
-- arrays. String quoting wraps without escaping (see html.lua note).
local Json = {}

Json.encode_num = function(v)
  return str(v)
end

Json.encode_bool = function(v)
  if v then return "true" end
  return "false"
end

Json.encode_str = function(v)
  return '"' .. v .. '"'
end

Json.encode_nil = function()
  return "null"
end

Json.encode_nums = function(items)
  local out = "["
  local i = 1
  while i <= #items do
    if i ~= 1 then out = out .. "," end
    out = out .. str(items[i])
    i = i + 1
  end
  return out .. "]"
end

Json.encode_strs = function(items)
  local out = "["
  local i = 1
  while i <= #items do
    if i ~= 1 then out = out .. "," end
    out = out .. '"' .. items[i] .. '"'
    i = i + 1
  end
  return out .. "]"
end

Json.encode_object = function(keys, vals)
  local out = "{"
  local i = 1
  while i <= #keys do
    if i ~= 1 then out = out .. "," end
    out = out .. '"' .. keys[i] .. '":' .. vals[i]
    i = i + 1
  end
  return out .. "}"
end

return Json
