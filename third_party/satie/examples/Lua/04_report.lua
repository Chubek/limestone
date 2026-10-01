-- `satisfiable` is useful when a full model is not needed.
local sat = lsatie.satisfiable("(a) & (~a)", "dpll")
if sat then
  print("SAT")
else
  print("UNSAT")
end
