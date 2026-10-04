local nu = require("lnuperf")
local months = {"jan","feb","mar","apr","may","jun","jul","aug","sep","oct","nov","dec"}

local ks = nu.Keyset.new()
for _, m in ipairs(months) do
  ks:add_string(m)
end

local t = nu.Table.new()
t:set_method("stdmeth")
t:set_target("stddef")
t:set_option("array_name", "month_mph")
t:build(ks)
t:emit_file("months_table.h")
print("wrote months_table.h")
