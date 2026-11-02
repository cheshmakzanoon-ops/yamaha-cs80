-- Unit test only: no DAW APIs, audio, screenshot or host acceptance receipt.
local function verify(add)
  local params, seen = {}, {}
  add(params, seen, "VOLUME", 1, "volume")
  assert(params.VOLUME.index == 1 and params.VOLUME.ident == "volume")
  add(params, seen, "Engine", 2, "engine")
  assert(params.Engine.index == 2 and params.Engine.ident == "engine")
  add(params, seen, "VOLUME", 3, "duplicate-volume")
  assert(params.VOLUME == false, "second duplicate must remain ambiguous")
  add(params, seen, "VOLUME", 4, "third-volume")
  assert(params.VOLUME == false, "third duplicate must not re-enable a parameter")
  assert(params.Engine.index == 2, "unrelated unique parameter must survive")
end
local path = "tests/hosts/reaper/parameter-inventory.lua"
verify(dofile(path))
local file = assert(io.open(path, "rb"))
local source = file:read("*a"); file:close()
local mutant, count = source:gsub("if seen%[name%] then", "if params[name] then")
assert(count == 1, "mutation must restore exactly the reviewed duplicate-name defect")
local ok, reason = pcall(verify, assert(load(mutant, "removed-duplicate-guard"))())
assert(not ok and tostring(reason):find("third duplicate must not re-enable", 1, true),
       "duplicate-name regression survived, or failed for an unrelated reason")
print("Five pure Lua inventory assertions passed; restored duplicate defect rejected; not host execution")
