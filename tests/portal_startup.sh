#!/bin/sh
# SPDX-License-Identifier: MIT
# xdg-desktop-portal activates `fuin --portal` while it starts and waits for it, so a backend that
# calls the portal during startup deadlocks both until D-Bus gives up (25 s). This starts the real
# frontend on a private session bus, with a portal-aware Qt platform theme, and requires the
# FileChooser version to answer within 2 s.
#   portal_startup.sh <fuin> <fuin.portal> <scratch dir>
set -eu
fuin=$1 portal_file=$2 work=$3
limit_ms=2000

frontend=
for f in /usr/lib/xdg-desktop-portal /usr/libexec/xdg-desktop-portal; do
    [ -x "$f" ] && frontend=$f && break
done
if [ -z "$frontend" ]; then
    echo "xdg-desktop-portal not installed; skipping"
    exit 77
fi

rm -rf "$work"
mkdir -p "$work/services" "$work/portals" "$work/home"
cp "$portal_file" "$work/portals/fuin.portal"
printf '[preferred]\ndefault=none\norg.freedesktop.impl.portal.FileChooser=fuin\n' > "$work/portals/portals.conf"
cat > "$work/bus.conf" <<CONF
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:tmpdir=$work</listen>
  <auth>EXTERNAL</auth>
  <servicedir>$work/services</servicedir>
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/>
    <allow eavesdrop="true"/>
    <allow own="*"/>
  </policy>
</busconfig>
CONF
cat > "$work/services/org.freedesktop.portal.Desktop.service" <<SVC
[D-BUS Service]
Name=org.freedesktop.portal.Desktop
Exec=/usr/bin/env XDG_DESKTOP_PORTAL_DIR=$work/portals $frontend
SVC
cat > "$work/services/org.freedesktop.impl.portal.desktop.fuin.service" <<SVC
[D-BUS Service]
Name=org.freedesktop.impl.portal.desktop.fuin
Exec=/usr/bin/env HOME=$work/home XDG_CONFIG_HOME=$work/home/.config QT_QPA_PLATFORM=offscreen QT_QPA_PLATFORMTHEME=xdgdesktopportal $fuin --portal
SVC

cat > "$work/run.sh" <<'RUN'
#!/bin/sh
limit_ms=$1
pid_of() {
    dbus-send --session --print-reply --dest=org.freedesktop.DBus / org.freedesktop.DBus.GetConnectionUnixProcessID "string:$1" 2>/dev/null \
        | awk '/uint32/ {print $2}'
}
start=$(date +%s%N)
reply=$(dbus-send --session --print-reply --reply-timeout=30000 --dest=org.freedesktop.portal.Desktop /org/freedesktop/portal/desktop \
    org.freedesktop.DBus.Properties.Get string:org.freedesktop.portal.FileChooser string:version 2>&1 | tail -n 1) || true
elapsed=$(( ($(date +%s%N) - start) / 1000000 ))
fuin_pid=$(pid_of org.freedesktop.impl.portal.desktop.fuin)
frontend_pid=$(pid_of org.freedesktop.portal.Desktop)
kill $fuin_pid $frontend_pid 2>/dev/null || true
echo "FileChooser version after ${elapsed} ms: $reply"
case $reply in
*"variant"*"uint32"*) ;;
*) echo "FAIL: no FileChooser version"; exit 1 ;;
esac
[ -n "$fuin_pid" ] || { echo "FAIL: fuin --portal does not own its bus name"; exit 1; }
[ "$elapsed" -lt "$limit_ms" ] || { echo "FAIL: slower than $limit_ms ms"; exit 1; }
RUN
chmod +x "$work/run.sh"
exec dbus-run-session --config-file="$work/bus.conf" -- "$work/run.sh" "$limit_ms"
