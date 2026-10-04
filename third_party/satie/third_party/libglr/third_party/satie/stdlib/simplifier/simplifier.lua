-- satie stdlib: simplifier (clause-list normalization over int arrays).
-- Clauses are int arrays; variables are positive ids. Returns a report
-- {clauses, tautologies, duplicates, subsumed}.
local Simplifier = {}

Simplifier.is_tautology = function(clause)
  local i = 1
  while i <= #clause do
    local j = i + 1
    while j <= #clause do
      if clause[i] == -clause[j] then return true end
      j = j + 1
    end
    i = i + 1
  end
  return false
end

Simplifier.dedup = function(clause)
  local out = {}
  local i = 1
  while i <= #clause do
    local seen = false
    local j = 1
    while j <= #out do
      if out[j] == clause[i] then seen = true end
      j = j + 1
    end
    if seen == false then
      out[#out + 1] = clause[i]
    end
    i = i + 1
  end
  return out
end

Simplifier.subsumes = function(a, b)
  if #a > #b then return false end
  if #a == 0 then return false end
  local i = 1
  while i <= #a do
    local found = false
    local j = 1
    while j <= #b do
      if b[j] == a[i] then found = true end
      j = j + 1
    end
    if found == false then return false end
    i = i + 1
  end
  return true
end

Simplifier.simplify = function(clauses)
  local report = {}
  report.tautologies = 0
  report.duplicates = 0
  report.subsumed = 0
  local clean = {}
  local i = 1
  while i <= #clauses do
    local clause = Simplifier.dedup(clauses[i])
    if Simplifier.is_tautology(clause) then
      report.tautologies = report.tautologies + 1
    else
      local dup = false
      local j = 1
      while j <= #clean do
        if #clean[j] == #clause then
          local same = true
          local k = 1
          while k <= #clause do
            if clean[j][k] ~= clause[k] then same = false end
            k = k + 1
          end
          if same then dup = true end
        end
        j = j + 1
      end
      if dup then
        report.duplicates = report.duplicates + 1
      else
        clean[#clean + 1] = clause
      end
    end
    i = i + 1
  end
  local kept = {}
  i = 1
  while i <= #clean do
    local sub = false
    local j = 1
    while j <= #clean do
      if j ~= i and Simplifier.subsumes(clean[j], clean[i]) then
        sub = true
      end
      j = j + 1
    end
    if sub then
      report.subsumed = report.subsumed + 1
    else
      kept[#kept + 1] = clean[i]
    end
    i = i + 1
  end
  report.clauses = kept
  return report
end

return Simplifier
