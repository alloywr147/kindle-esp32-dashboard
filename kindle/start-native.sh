#!/bin/sh
set -e
export DISPLAY=:0
D=/mnt/us/kt2-dashboard/native
[ -z "$(pidof reader.lua)" ] || exit 10
[ ! -f "$D/ui-active" ] || exit 11
lipc-get-prop com.lab126.powerd preventScreenSaver > "$D/ui-original-prevent"
cat /proc/sys/kernel/random/boot_id > "$D/ui-boot-id"
TOKEN=$(cat /proc/sys/kernel/random/uuid)
echo "$TOKEN" > "$D/ui-active"
cat > "$D/ui-restore.sh" <<'RESTORE_EOF'
#!/bin/sh
D=/mnt/us/kt2-dashboard/native
[ -f "$D/ui-active" ] || exit 0
[ "$(cat /proc/sys/kernel/random/boot_id)" = "$(cat "$D/ui-boot-id")" ] || exit 1
# Terminate only this isolated KOReader instance. Its official wrapper restores the UI.
for p in /proc/[0-9]*/cwd; do
  [ "$(readlink "$p" 2>/dev/null)" = "$D/koreader" ] || continue
  pid=${p#/proc/};pid=${pid%/cwd}
  cmd=$(tr '\000' ' ' < /proc/$pid/cmdline 2>/dev/null)
  case "$cmd" in *reader.lua*) kill -TERM "$pid" 2>/dev/null || :;; esac
done
sleep 3
killall -CONT awesome 2>/dev/null || :
lipc-set-prop com.lab126.pillow disableEnablePillow enable >/dev/null 2>&1 || :
ORIG=$(cat "$D/ui-original-prevent")
case "$ORIG" in 0|1) lipc-set-prop com.lab126.powerd preventScreenSaver "$ORIG";; esac
lipc-set-prop com.lab126.appmgrd start app://com.lab126.booklet.home >/dev/null 2>&1 || :
rm -f "$D/ui-active"
echo UI_GUARD_RESTORED
RESTORE_EOF
cat > "$D/ui-guard.sh" <<'GUARD_EOF'
#!/bin/sh
D=/mnt/us/kt2-dashboard/native
TOKEN=$1
sleep 30
while [ "$(cat "$D/ui-active" 2>/dev/null)" = "$TOKEN" ]; do
  PID=$(cat "$D/ui-wrapper.pid" 2>/dev/null)
  if [ -z "$PID" ] || ! kill -0 "$PID" 2>/dev/null; then
    sh "$D/ui-restore.sh" >> "$D/ui-guard.log" 2>&1
    exit
  fi
  sleep 30
done
GUARD_EOF
nohup sh "$D/ui-guard.sh" "$TOKEN" >/dev/null 2>&1 </dev/null &
GUARD_PID=$!
kill -0 "$GUARD_PID"
# Guard is running BEFORE the first runtime-setting mutation.
LAUNCHED=0
trap '[ "$LAUNCHED" = 1 ] || sh "$D/ui-restore.sh"' 0
lipc-set-prop com.lab126.powerd preventScreenSaver 1
:
cd "$D/koreader"
./luajit -e 'require("setupkoenv"); local j=require("json"); local f=assert(io.open("kt2-config.json","r")); local c=j.decode(f:read("*a")); f:close(); c.test_duration=0; local o=assert(io.open("kt2-config.tmp","w")); o:write(j.encode(c)); o:close(); assert(os.rename("kt2-config.tmp","kt2-config.json"))'

printf 'return { ["language"] = "zh_CN", ["start_with"] = "filemanager" }\n' > settings.reader.lua
touch kt2-test-autostart
nohup sh -c 'sh ./koreader.sh --asap /mnt/us/kt2-dashboard/native; sh ../ui-restore.sh' > "$D/launcher.log" 2>&1 </dev/null &
echo $! > "$D/ui-wrapper.pid"
LAUNCHED=1
echo KT2_CONTINUOUS_LAUNCHED_NO_DEADLINE
