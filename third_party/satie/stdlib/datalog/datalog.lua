-- satie stdlib: datalog (naive bottom-up over tuple arrays).
-- Facts: {rel = {{"a", "b"}, ...}}. Rules: {head = {"rel", {"X", "c"}},
-- body = {{"rel2", {"X", "Y"}}, ...}} where uppercase-leading args are
-- variables. evaluate(db, rules, cap) runs to fixpoint or the cap.
local Datalog = {}

Datalog.fact_key = function(tuple)
  local key = ""
  local i = 1
  while i <= #tuple do
    if i ~= 1 then key = key .. "\1" end
    key = key .. tuple[i]
    i = i + 1
  end
  return key
end

Datalog.is_var = function(arg)
  local first = arg
  return first == "X" or first == "Y" or first == "Z" or first == "W" or
    first == "U" or first == "V"
end

Datalog.unify = function(pattern, tuple, binding)
  if #pattern ~= #tuple then return nil end
  local out = {}
  local k = 1
  while k <= #binding do
    out[#out + 1] = {binding[k][1], binding[k][2]}
    k = k + 1
  end
  local i = 1
  while i <= #pattern do
    if Datalog.is_var(pattern[i]) then
      local found = nil
      local j = 1
      while j <= #out do
        if out[j][1] == pattern[i] then found = out[j][2] end
        j = j + 1
      end
      if found == nil then
        out[#out + 1] = {pattern[i], tuple[i]}
      else
        if found ~= tuple[i] then return nil end
      end
    else
      if pattern[i] ~= tuple[i] then return nil end
    end
    i = i + 1
  end
  return out
end

Datalog.evaluate = function(db, rules, cap)
  local step = 0
  while step < cap do
    local changed = false
    local r = 1
    while r <= #rules do
      local rule = rules[r]
      local partials = {{}}
      local b = 1
      while b <= #rule.body do
        local atom = rule.body[b]
        local facts = db[atom[1]]
        if facts == nil then
          partials = {}
          break
        end
        local next = {}
        local p = 1
        while p <= #partials do
          local f = 1
          while f <= #facts do
            local extended = Datalog.unify(atom[2], facts[f], partials[p])
            if extended ~= nil then
              next[#next + 1] = extended
            end
            f = f + 1
          end
          p = p + 1
        end
        partials = next
        b = b + 1
      end
      local p = 1
      while p <= #partials do
        local head = {}
        local ok = true
        local i = 1
        while i <= #rule.head[2] do
          if Datalog.is_var(rule.head[2][i]) then
            local found = nil
            local j = 1
            while j <= #partials[p] do
              if partials[p][j][1] == rule.head[2][i] then
                found = partials[p][j][2]
              end
              j = j + 1
            end
            if found == nil then ok = false break end
            head[#head + 1] = found
          else
            head[#head + 1] = rule.head[2][i]
          end
          i = i + 1
        end
        if ok then
          local rel = rule.head[1]
          if db[rel] == nil then db[rel] = {} end
          local key = Datalog.fact_key(head)
          local seen = false
          local f = 1
          while f <= #db[rel] do
            if Datalog.fact_key(db[rel][f]) == key then seen = true end
            f = f + 1
          end
          if seen == false then
            db[rel][#db[rel] + 1] = head
            changed = true
          end
        end
        p = p + 1
      end
      r = r + 1
    end
    if changed == false then return db end
    step = step + 1
  end
  return db
end

Datalog.query = function(db, rel, tuple)
  local facts = db[rel]
  if facts == nil then return false end
  local key = Datalog.fact_key(tuple)
  local i = 1
  while i <= #facts do
    if Datalog.fact_key(facts[i]) == key then return true end
    i = i + 1
  end
  return false
end

return Datalog
