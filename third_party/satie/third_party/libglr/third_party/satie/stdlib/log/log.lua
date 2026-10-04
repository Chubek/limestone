-- satie stdlib: log (leveled logging over print; explicit state).
local Log = {}

Log.DEBUG = 0
Log.INFO = 1
Log.WARNING = 2
Log.ERROR = 3

Log.new = function(scope)
  return {scope = scope, level = Log.INFO, counts = {0, 0, 0, 0}}
end

Log.set_level = function(st, level)
  st.level = level
end

Log.emit = function(st, level, message)
  if level < st.level then return end
  st.counts[level + 1] = st.counts[level + 1] + 1
  print("[" .. st.scope .. "] " .. message)
end

Log.debug = function(st, message)
  Log.emit(st, Log.DEBUG, message)
end

Log.info = function(st, message)
  Log.emit(st, Log.INFO, message)
end

Log.warning = function(st, message)
  Log.emit(st, Log.WARNING, message)
end

Log.error = function(st, message)
  Log.emit(st, Log.ERROR, message)
end

Log.count = function(st, level)
  return st.counts[level + 1]
end

return Log
