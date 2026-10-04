-- satie stdlib: fmt (string building without the string library).
local Fmt = {}

Fmt.join = function(items, sep)
  local out = ""
  local i = 1
  while i <= #items do
    if i ~= 1 then out = out .. sep end
    out = out .. items[i]
    i = i + 1
  end
  return out
end

Fmt.repeat_str = function(text, count)
  local out = ""
  local i = 1
  while i <= count do
    out = out .. text
    i = i + 1
  end
  return out
end

Fmt.pad_left = function(text, width)
  local out = text
  while #out < width do
    out = " " .. out
  end
  return out
end

Fmt.pad_right = function(text, width)
  local out = text
  while #out < width do
    out = out .. " "
  end
  return out
end

Fmt.quote = function(text)
  return '"' .. text .. '"'
end

Fmt.lines = function(items)
  return Fmt.join(items, "\n") .. "\n"
end

return Fmt
