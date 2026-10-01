-- satie stdlib: yaml (block-style emitter for flat data).
-- Mappings emit from parallel key/value string arrays; sequences from
-- string arrays; nested blocks from pre-rendered strings with indent.
local Yaml = {}

Yaml.mapping = function(keys, vals)
  local out = ""
  local i = 1
  while i <= #keys do
    out = out .. keys[i] .. ": " .. vals[i] .. "\n"
    i = i + 1
  end
  return out
end

Yaml.sequence = function(items)
  local out = ""
  local i = 1
  while i <= #items do
    out = out .. "- " .. items[i] .. "\n"
    i = i + 1
  end
  return out
end

Yaml.nested = function(key, body, indent)
  local pad = ""
  local i = 1
  while i <= indent do
    pad = pad .. " "
    i = i + 1
  end
  return key .. ":\n" .. pad .. body
end

return Yaml
