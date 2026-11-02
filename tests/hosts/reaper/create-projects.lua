-- Runs inside the pinned, real REAPER executable. No synthetic host wrapper.
local out = assert(os.getenv("CS80_REAPER_OUT"), "CS80_REAPER_OUT is required")
local c = dofile(out .. "/contract.lua")
local register_parameter = dofile(assert(os.getenv("CS80_REAPER_FIXTURE")) .. "/parameter-inventory.lua")
local report = {schema=c.schema, host_version=reaper.GetAppVersion(), projects={}, checks={}}
local function json(v)
  if type(v) == "string" then
    return '"' .. v:gsub('[%z\1-\31\\"]', function(ch)
      return string.format("\\u%04x", string.byte(ch))
    end) .. '"'
  elseif type(v) == "number" or type(v) == "boolean" then return tostring(v)
  elseif type(v) == "table" then
    local entries = {}
    if #v > 0 or next(v) == nil then
      for _, value in ipairs(v) do entries[#entries+1] = json(value) end
      return "[" .. table.concat(entries, ",") .. "]"
    end
    for key, value in pairs(v) do entries[#entries+1] = json(key) .. ":" .. json(value) end
    table.sort(entries)
    return "{" .. table.concat(entries, ",") .. "}"
  end
  error("unsupported JSON value: " .. type(v))
end
local function write(name, value)
  local f = assert(io.open(out .. "/" .. name, "wb"))
  f:write(value); f:close()
end
local function check(ok, why)
  report.checks[#report.checks+1] = {ok=not not ok, description=why}
  assert(ok, why)
end
local function close_host()
  reaper.Main_OnCommand(40004, 0) -- File: Quit
end
local function fail(err)
  report.error = tostring(err)
  write("host-observations.json", json(report))
  write("FAILED", tostring(err))
  -- Keep the genuine failing UI available for the external diagnostic capture.
end
local track, fx, params
local function config(key)
  local ok, value = reaper.TrackFX_GetNamedConfigParm(track, fx, key)
  check(ok, "host query " .. key)
  return value
end
local function parameter(name)
  check(type(params[name]) == "table", "unique host parameter exists: " .. name)
  return params[name].index
end
local function value(name)
  return reaper.TrackFX_GetParamNormalized(track, fx, parameter(name))
end
local function set(name, v)
  check(reaper.TrackFX_SetParamNormalized(track, fx, parameter(name), v), "set " .. name)
  check(math.abs(value(name)-v) <= c.limits.parameter_absolute_error, "read back " .. name)
end
local function identify()
  check(reaper.CountTracks(0) == 1, "exactly one instrument track")
  track = reaper.GetTrack(0, 0); fx = 0
  check(reaper.TrackFX_GetCount(track) == 1, "exactly one host FX instance")
  check(config("fx_type"):find("VST3", 1, true) ~= nil, "actual VST3 instance")
  check(config("fx_name"):find("CS-80 Reference", 1, true) ~= nil, "expected plugin name")
  check(config("is_instrument") == "1", "host classifies plugin as instrument")
  params = {}
  local seen = {}
  local inventory = {}
  for p=0,reaper.TrackFX_GetNumParams(track, fx)-1 do
    local ok, name = reaper.TrackFX_GetParamName(track, fx, p)
    local id_ok, ident = reaper.TrackFX_GetParamIdent(track, fx, p)
    check(ok and id_ok, "read parameter identity " .. p)
    check(reaper.TrackFX_GetParamFromIdent(track, fx, ident) == p, "host parameter identity round trip " .. p)
    inventory[#inventory+1] = {index=p, name=name, ident=ident}
    -- Duplicate names are not silently resolved for the parameters under test.
    register_parameter(params, seen, name, p, ident)
  end
  report.parameters = inventory
  report.fx_ident = config("fx_ident")
  report.fx_name = config("fx_name")
  report.pdc_samples = tonumber(config("pdc"))
  report.chain_reported_pdc_samples = tonumber(config("chain_pdc_reporting"))
  check(report.pdc_samples ~= nil and report.pdc_samples >= 0, "finite nonnegative host PDC query")
end
local settings = {VOLUME=c.render.volume_normalized, ["VOICE DISPERSION"]=0,
                  ["ANALOG DRIFT"]=0, ["ANALOG NOISE"]=0}
if c.render.chorus_on_normalized ~= nil then
  settings["CHORUS ON"] = c.render.chorus_on_normalized
end
local function state_readback()
  local result = {}
  for name, expected in pairs(settings) do
    result[name] = value(name)
    check(math.abs(result[name]-expected) <= c.limits.parameter_absolute_error, "restored parameter " .. name)
  end
  return result
end
local function midi_readback(expected_count)
  if expected_count == 0 then
    check(reaper.CountTrackMediaItems(track) == 0, "no-MIDI negative control contains no items")
    return {}
  end
  check(reaper.CountTrackMediaItems(track) == 1, "one persisted MIDI item")
  local item = reaper.GetTrackMediaItem(track, 0)
  local take = reaper.GetActiveTake(item)
  check(take ~= nil and reaper.TakeIsMIDI(take), "persisted take is MIDI")
  local _, count, cc, text = reaper.MIDI_CountEvts(take)
  check(count == expected_count and cc == 0 and text == 0, "exact persisted MIDI event inventory")
  local notes = {}
  for i=0,count-1 do
    local ok, _, muted, startppq, endppq, channel, pitch, velocity = reaper.MIDI_GetNote(take, i)
    local start = reaper.MIDI_GetProjTimeFromPPQPos(take, startppq)
    local finish = reaper.MIDI_GetProjTimeFromPPQPos(take, endppq)
    check(ok and not muted and channel == 0 and pitch == c.render.notes[i+1]
      and velocity == c.render.velocity and math.abs(start-c.render.note_start_seconds) < 1e-8
      and math.abs(finish-c.render.note_end_seconds) < 1e-8, "persisted MIDI note " .. i)
    notes[#notes+1] = {pitch=pitch, velocity=velocity, start_seconds=start, end_seconds=finish}
  end
  return notes
end
local function render_settings(name)
  local numeric = {PROJECT_SRATE=c.render.sample_rate, PROJECT_SRATE_USE=1,
    RENDER_SETTINGS=0, RENDER_BOUNDSFLAG=0, RENDER_CHANNELS=c.render.channels,
    RENDER_SRATE=c.render.sample_rate, RENDER_STARTPOS=0, RENDER_ENDPOS=c.render.seconds,
    RENDER_TAILFLAG=0, RENDER_ADDTOPROJ=0, RENDER_DITHER=16, RENDER_NORMALIZE=0}
  for key,v in pairs(numeric) do reaper.GetSetProjectInfo(0, key, v, true) end
  for key,v in pairs({RENDER_FILE=out, RENDER_PATTERN=name, RENDER_FORMAT="evaw", RENDER_FORMAT2=""}) do
    check(reaper.GetSetProjectInfo_String(0, key, v, true), "set render " .. key)
  end
end
local function save(name)
  render_settings(name)
  reaper.Main_SaveProjectEx(0, out .. "/" .. name .. ".rpp", 8)
  local f = io.open(out .. "/" .. name .. ".rpp", "rb")
  check(f ~= nil, "host saved " .. name .. ".rpp")
  f:close()
end
local function reopen(name)
  reaper.Main_openProject("noprompt:" .. out .. "/" .. name .. ".rpp")
  identify()
end
local function envelope_readback()
  local env = reaper.GetFXEnvelope(track, fx, parameter("VOLUME"), false)
  check(env ~= nil, "host restored volume automation")
  check(reaper.CountEnvelopePoints(env) == #c.render.automation_points, "exact automation point inventory")
  local points = {}
  for i, expected in ipairs(c.render.automation_points) do
    local ok, time, v, shape = reaper.GetEnvelopePoint(env, i-1)
    check(ok and math.abs(time-expected[1]) < 1e-8 and math.abs(v-expected[2]) < 1e-8
      and shape == 0, "persisted linear envelope point " .. i)
    points[#points+1] = {time, v}
  end
  return points
end
local function create()
  check(report.host_version == c.host.version .. "/" .. c.host.platform, "exact running host version and architecture")
  report.audio_device = {}
  for _, key in ipairs({"MODE", "SRATE", "BSIZE"}) do
    local ok, actual = reaper.GetAudioDeviceInfo(key)
    report.audio_device[key] = {available=ok, value=actual}
  end
  check(reaper.CountTracks(0) == 0, "isolated empty starting project")
  local matches = {}
  for i=0,10000 do
    local ok, name, ident = reaper.EnumInstalledFX(i)
    if not ok then break end
    if name:find("CS-80 Reference", 1, true) and name:find("VST3", 1, true) then
      matches[#matches+1] = {name=name, ident=ident}
    end
  end
  report.scan_matches = matches
  check(#matches == 1, "one scanned VST3 matching CS-80 Reference")
  reaper.InsertTrackAtIndex(0, true)
  track = reaper.GetTrack(0, 0)
  fx = reaper.TrackFX_AddByName(track, matches[1].name, false, 1)
  check(fx == 0, "real host instantiated scanned VST3")
  identify()
  check(value("Engine") == 1, "fresh host instance uses Circuit engine")
  for name,v in pairs(settings) do set(name, v) end
  report.before_save = state_readback()
  reaper.GetSetMediaTrackInfo_String(track, "P_NAME", "CS-80 real-host smoke", true)
  reaper.SetTrackAutomationMode(track, 1) -- read
  local item = reaper.CreateNewMIDIItemInProj(track, 0, c.render.seconds, false)
  check(item ~= nil, "created actual host MIDI item")
  local take = reaper.GetActiveTake(item)
  local start = reaper.MIDI_GetPPQPosFromProjTime(take, c.render.note_start_seconds)
  local finish = reaper.MIDI_GetPPQPosFromProjTime(take, c.render.note_end_seconds)
  for _, pitch in ipairs(c.render.notes) do
    check(reaper.MIDI_InsertNote(take, false, false, start, finish, 0, pitch, c.render.velocity, true), "insert MIDI note " .. pitch)
  end
  reaper.MIDI_Sort(take)
  save("baseline")
  report.projects.baseline = {parameters=state_readback(), notes=midi_readback(#c.render.notes)}
  -- A genuine state restoration probe: change live parameters, then reopen.
  set("VOLUME", 0.2); set("VOICE DISPERSION", 0.7)
  report.perturbed = {VOLUME=value("VOLUME"), ["VOICE DISPERSION"]=value("VOICE DISPERSION")}
  reopen("baseline")
  report.after_reopen = state_readback()
  save("restored")
  report.projects.restored = {parameters=state_readback(), notes=midi_readback(#c.render.notes)}
  local env = reaper.GetFXEnvelope(track, fx, parameter("VOLUME"), true)
  check(env ~= nil, "create actual host volume envelope")
  reaper.DeleteEnvelopePointRange(env, -1, c.render.seconds+1)
  for _, point in ipairs(c.render.automation_points) do
    check(reaper.InsertEnvelopePoint(env, point[1], point[2], 0, 0, false, true), "insert automation point")
  end
  reaper.Envelope_SortPoints(env)
  save("automated"); reopen("automated")
  report.projects.automated = {notes=midi_readback(#c.render.notes), envelope=envelope_readback()}
  reopen("baseline")
  check(reaper.DeleteTrackMediaItem(track, reaper.GetTrackMediaItem(track, 0)), "remove MIDI for negative control")
  save("no-midi")
  report.projects["no-midi"] = {parameters=state_readback(), notes=midi_readback(0)}
  reopen("automated")
  reaper.TrackFX_Show(track, fx, 3)
  reaper.UpdateArrange()
  local deadline = reaper.time_precise() + 30
  local function await_editor()
    if reaper.TrackFX_GetOpen(track, fx) then
      report.editor_open = true
      write("host-observations.json", json(report))
      write("READY_FOR_SCREENSHOT", "actual host reports plugin editor open\n")
      local function await_capture()
        local ack = io.open(out .. "/SCREENSHOT_CAPTURED", "rb")
        if ack then
          ack:close()
          if c.schema == "cs80-reaper-smoke-v2-dry" then
            report.editor_open_after_dismissal = reaper.TrackFX_GetOpen(track, fx)
            if not report.editor_open_after_dismissal then
              fail("host editor closed during tour dismissal"); return
            end
            write("host-observations.json", json(report))
          end
          reaper.TrackFX_Show(track, fx, 2)
          reaper.Main_SaveProject(0, false)
          write("HOST_SETUP_COMPLETE", "state, MIDI, envelope and editor observations recorded\n")
          close_host()
        elseif reaper.time_precise() > deadline then fail("screenshot acknowledgement timeout")
        else reaper.defer(await_capture) end
      end
      reaper.defer(await_capture)
    elseif reaper.time_precise() > deadline then fail("host editor did not open")
    else reaper.defer(await_editor) end
  end
  reaper.defer(await_editor)
end
local ok, err = xpcall(create, debug.traceback)
if not ok then fail(err) end
