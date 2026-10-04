-- satie stdlib: sys (static build metadata; live queries are C++-only).
local Sys = {}

Sys.version = "0.1.0"
Sys.build = "satie 0.1.0 cxx20"

Sys.version_string = function()
  return Sys.version
end

return Sys
