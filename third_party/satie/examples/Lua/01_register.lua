-- Register a command that can be invoked by C++ or another Lua plugin.
lsatie.register("greet", function(args)
  local name = args[1] or "world"
  return "hello " .. name
end)

print(lsatie.invoke("greet", {"Satie"}))
