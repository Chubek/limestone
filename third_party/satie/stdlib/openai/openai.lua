-- satie stdlib: openai (chat payload builders; no network involved).
local Openai = {}

Openai.message = function(role, content)
  return {role, content}
end

Openai.chat_payload = function(model, messages)
  local items = ""
  local i = 1
  while i <= #messages do
    if i ~= 1 then items = items .. "," end
    items = items .. '{"role":"' .. messages[i][1] ..
      '","content":"' .. messages[i][2] .. '"}'
    i = i + 1
  end
  return '{"model":"' .. model .. '","messages":[' .. items .. "]}"
end

return Openai
