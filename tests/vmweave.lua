local source,output=arg[1],arg[2]
package.path=source.."/vmweave/?.lua;"..package.path
local vmweave=require("vmweave")
local function invalid(fn) assert(not pcall(fn)) end
local function make(reverse)
  local vm=vmweave.vm("Example")
  if reverse then vm:state("pc",{type="u64"}):state("acc",{type="i64"})
  else vm:state("acc",{type="i64"}):state("pc",{type="u64"}) end
  vm:instruction("increment",{body="  vm->acc += 2; vm->pc += 1;"})
  vm:instruction("empty")
  return vm
end
local vm=make(false)
assert(vm:emit()==make(true):emit())
local metadata={type="u8"};vm:state("buffer",{type="u8",count=4});vm:state("copied",metadata);metadata.type="invalid"
assert(vm:emit():find("uint8_t copied;",1,true))
invalid(function() vmweave.vm("bad-name") end)
invalid(function() vmweave.vm("int") end)
invalid(function() vmweave.vm("x\n*/") end)
invalid(function() vm:state("pc",{type="u64"}) end)
invalid(function() vm:state("x",{type="invalid"}) end)
invalid(function() vm:state("x",{type="u8",count=0}) end)
invalid(function() vm:state("x",{type="u8",count=1.5}) end)
invalid(function() vm:state("x",{type="u8",unexpected=true}) end)
invalid(function() vm:instruction("increment") end)
invalid(function() vm:instruction("new",{body=7}) end)
invalid(function() vm:instruction("new",{body="\0"}) end)
invalid(function() vm.state("x",{type="u8"}) end)
local file=assert(io.open(output,"w"));file:write(vm:emit())
file:write("\nint main(void) { Example_vm vm = {0}; Example_increment(&vm); Example_empty(&vm); return vm.acc == 2 && vm.pc == 1 ? 0 : 1; }\n")
assert(file:close())
