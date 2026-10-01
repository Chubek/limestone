-- satie stdlib: graphlib (directed graphs over integer nodes).
-- Graphs are explicit state tables ({succ = {}}); every operation takes
-- the graph first, so no closures are needed.
local Graphlib = {}

Graphlib.new = function()
  return {succ = {}}
end

Graphlib.add_edge = function(g, from, to)
  if g.succ[from] == nil then g.succ[from] = {} end
  if g.succ[to] == nil then g.succ[to] = {} end
  local known = false
  local i = 1
  while i <= #g.succ[from] do
    if g.succ[from][i] == to then known = true end
    i = i + 1
  end
  if known == false then
    g.succ[from][#g.succ[from] + 1] = to
  end
end

Graphlib.bfs = function(g, start)
  local order = {}
  local seen = {}
  local queue = {start}
  seen[start] = true
  local head = 1
  while head <= #queue do
    local node = queue[head]
    head = head + 1
    order[#order + 1] = node
    local nexts = g.succ[node]
    if nexts ~= nil then
      local i = 1
      while i <= #nexts do
        if seen[nexts[i]] == nil then
          seen[nexts[i]] = true
          queue[#queue + 1] = nexts[i]
        end
        i = i + 1
      end
    end
  end
  return order
end

Graphlib.has_path = function(g, from, to)
  local order = Graphlib.bfs(g, from)
  local i = 1
  while i <= #order do
    if order[i] == to then return true end
    i = i + 1
  end
  return false
end

return Graphlib
