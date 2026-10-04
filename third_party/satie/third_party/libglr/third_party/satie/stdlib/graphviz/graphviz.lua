-- satie stdlib: graphviz (DOT text builder; explicit state).
local Graphviz = {}

Graphviz.new = function(name)
  return {name = name, lines = {}}
end

Graphviz.node = function(g, id, label)
  if label == nil then label = id end
  g.lines[#g.lines + 1] = '  "' .. id .. '" [label="' .. label .. '"];'
end

Graphviz.edge = function(g, from, to, label)
  local line = '  "' .. from .. '" -> "' .. to .. '"'
  if label ~= nil then
    line = line .. ' [label="' .. label .. '"]'
  end
  g.lines[#g.lines + 1] = line .. ';'
end

Graphviz.dump = function(g)
  local out = "digraph " .. g.name .. " {\n"
  local i = 1
  while i <= #g.lines do
    out = out .. g.lines[i] .. "\n"
    i = i + 1
  end
  return out .. "}\n"
end

return Graphviz
