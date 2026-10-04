-- satie stdlib: logsynth (JSON-lines writer; explicit state, caller fields).
-- Fields are preformatted '"key":value' strings; see field_str/int/bool.
local Logsynth = {}

Logsynth.new = function()
  return {lines = {}}
end

Logsynth.field_str = function(key, value)
  return '"' .. key .. '":"' .. value .. '"'
end

Logsynth.field_int = function(key, value)
  return '"' .. key .. '":' .. str(value)
end

Logsynth.field_bool = function(key, value)
  if value then
    return '"' .. key .. '":true'
  end
  return '"' .. key .. '":false'
end

Logsynth.emit = function(st, name, fields)
  local line = '{"event":"' .. name .. '"'
  local i = 1
  while i <= #fields do
    line = line .. "," .. fields[i]
    i = i + 1
  end
  st.lines[#st.lines + 1] = line .. "}"
end

Logsynth.dump = function(st)
  local out = ""
  local i = 1
  while i <= #st.lines do
    out = out .. st.lines[i] .. "\n"
    i = i + 1
  end
  return out
end

return Logsynth
