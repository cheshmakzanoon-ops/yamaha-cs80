-- Pure helper shared with a host-free Lua unit test. Any duplicate stays ambiguous.
return function(params, seen, name, index, ident)
  if seen[name] then params[name] = false else params[name] = {index=index, ident=ident} end
  seen[name] = true
end
