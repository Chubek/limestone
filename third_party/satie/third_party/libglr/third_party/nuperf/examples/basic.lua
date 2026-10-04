local nu = require("lnuperf")

local ks = nu.Keyset.new()
ks:add_string("apple")
ks:add_string("banana")
ks:add_string("cherry")

local t = nu.Table.new()
t:set_method("stdmeth")
t:set_target("stddef")
t:build(ks)

local out = t:emit_buffer()
print(out)
