-- satie stdlib: variables (scoped fresh-variable supply; explicit state).
local Variables = {}

Variables.new = function(first)
  if first == nil then first = 1 end
  return {next = first}
end

Variables.fresh = function(st)
  local id = st.next
  st.next = st.next + 1
  return id
end

Variables.checkpoint = function(st)
  return st.next
end

Variables.rollback = function(st, point)
  st.next = point
end

return Variables
