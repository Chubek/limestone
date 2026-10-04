-- Solve the formula with Satie's CDCL engine.
local result = lsatie.solve("(x1 | x2) & (~x1 | x3) & (~x2 | ~x3)", "cdcl")
print(result.status)
print(result.engine)
