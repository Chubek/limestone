-- satie stdlib: latex (document builders; no escaping, see html.lua note).
local Latex = {}

Latex.math = function(body)
  return "\\(" .. body .. "\\)"
end

Latex.frac = function(num, den)
  return "\\frac{" .. num .. "}{" .. den .. "}"
end

Latex.env = function(name, body)
  return "\\begin{" .. name .. "}\n" .. body .. "\\end{" .. name .. "}\n"
end

Latex.document = function(body)
  return "\\documentclass{article}\n\\begin{document}\n" .. body .. "\\end{document}\n"
end

return Latex
