-- List the commands exposed by the C++ and Lua plugin hosts.
for index = 1, #lsatie.commands() do
  print(lsatie.commands()[index])
end
