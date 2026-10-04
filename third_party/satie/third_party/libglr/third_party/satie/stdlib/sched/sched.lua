-- satie stdlib: sched (deterministic dependency scheduler).
-- Tasks: {name = "a", deps = {"b"}, run = function() ... end}.
-- run_all returns results in execution order ({name, value} pairs).
local Sched = {}

Sched.run_all = function(tasks)
  local done = {}
  local results = {}
  local remaining = #tasks
  while remaining > 0 do
    local pick = nil
    local i = 1
    while i <= #tasks do
      local name = tasks[i].name
      if done[name] == nil then
        local ready = true
        local d = 1
        while d <= #tasks[i].deps do
          if done[tasks[i].deps[d]] == nil then ready = false end
          d = d + 1
        end
        if ready then
          if pick == nil then pick = i end
          if tasks[i].name < tasks[pick].name then pick = i end
        end
      end
      i = i + 1
    end
    if pick == nil then
      return nil
    end
    local value = tasks[pick].run()
    done[tasks[pick].name] = true
    results[#results + 1] = {tasks[pick].name, value}
    remaining = remaining - 1
  end
  return results
end

return Sched
