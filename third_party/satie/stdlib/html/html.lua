-- satie stdlib: html (tiny tag/table/page builders).
-- NOTE: no escaping is performed (the dialect cannot scan strings);
-- callers must pass pre-escaped text for untrusted input.
local Html = {}

Html.tag = function(name, body)
  return "<" .. name .. ">" .. body .. "</" .. name .. ">"
end

Html.table = function(headers, rows)
  local head = ""
  local i = 1
  while i <= #headers do
    head = head .. Html.tag("th", headers[i])
    i = i + 1
  end
  local body = ""
  local r = 1
  while r <= #rows do
    local cells = ""
    local c = 1
    while c <= #rows[r] do
      cells = cells .. Html.tag("td", rows[r][c])
      c = c + 1
    end
    body = body .. Html.tag("tr", cells)
    r = r + 1
  end
  return Html.tag("table", Html.tag("tr", head) .. body)
end

Html.page = function(title, body)
  return "<!DOCTYPE html>\n<html><head><title>" .. title ..
    "</title></head><body>" .. body .. "</body></html>\n"
end

return Html
