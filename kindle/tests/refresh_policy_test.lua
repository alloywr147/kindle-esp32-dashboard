local model=dofile('plugins/kt2board.koplugin/model.lua')
local function key(revision,bucket,pending)
    return model.refreshVersion(revision..':'..bucket..':'..(pending or 0))
end
-- Five-minute ticks inside one hour must not reload the display.
for bucket=120,130 do assert(key(7,bucket)==key(7,bucket+1)) end
assert(key(7,131)~=key(7,132),'hour boundary must update')
assert(key(7,120)~=key(8,120),'todo edits must update immediately')
assert(key(7,120,0)~=key(7,120,1),'manual weather request must update')
-- Midnight in Suzhou is also an hour boundary in the existing UTC time bucket.
assert(key(7,191)~=key(7,192),'midnight must update date and almanac')
assert(model.refreshVersion('bad')==nil,'malformed version must not be accepted')
print('REFRESH_POLICY_OK: five-minute suppression, hourly, edits, manual weather, midnight')
