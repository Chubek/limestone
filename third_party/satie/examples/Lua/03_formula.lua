-- Inspect the CNF generated from Satie's textual formula format.
local formula = "(feature_a | feature_b) & (~feature_a | feature_c)"
local parsed = lsatie.parse(formula)
print(parsed.variables)
print(parsed.clauses)
print(parsed.dimacs)
