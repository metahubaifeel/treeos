#!/bin/bash
# Inside the detached service only. CWD is a prepared, private game session.
set -eu
bin=${1:?}; player_bin="$bin/ruffle-keys"; quit="$bin/flash-quit-watch"
cursor="$bin/libruffle_cursorfix.aarch64.so"
mark(){ printf '%s phase=%s %s\n' "$(date -Is)" "$1" "${2:-}"; }
mark preflight
trap 'rc=$?; mark worker-exit "status=$rc"' EXIT
[[ -x $player_bin && -r $bin/quick-menu.so && -r $quit && -r $cursor && -f ruffle_data/movie.swf && -r $RUFFLE_PROFILE ]]
if pgrep -x 'ruffle-native\.a|ruffle-native-m|ruffle-r1-test|ruffle-keys' >/dev/null; then echo 'another player owns the display'; exit 1; fi
export SDL_GAMECONTROLLERCONFIG='1900a4dd726b333536322d6a6f797300,rk3562-joystick,b:b0,x:b2,y:b3,back:b8,start:b9,guide:b10,leftshoulder:b4,rightshoulder:b1,paddle1:b5,paddle2:b6,paddle3:b7,leftstick:b11,rightstick:b12,dpup:b13,dpdown:b14,dpleft:b15,dpright:b16,leftx:a0,lefty:a1,rightx:a3,righty:a4,platform:Linux,'
panel=/sys/class/backlight/backlight; read -r was <"$panel/brightness" || was=0
# Only recover ES if this worker actually claimed the display.
touch /run/treeos-flash-play/owns-display
mark stop-es
sudo -n systemctl stop emulationstation.service
mark wait-drm
for ((i=0;i<60;i++)); do sudo -n fuser -s /dev/dri/card0 || break; sleep .2; done
if sudo -n fuser -s /dev/dri/card0; then echo 'display busy'; exit 1; fi
if [[ $(<"$panel/brightness") = 0 ]]; then
  [[ $was =~ ^[1-9][0-9]*$ ]] || was=$(head -1 /var/lib/systemd/backlight/platform-backlight:backlight:backlight || true)
  max=$(<"$panel/max_brightness"); if [[ $was =~ ^[1-9][0-9]*$ && $max =~ ^[1-9][0-9]*$ ]] && ((was<=max)); then printf '%s\n' "$was"|sudo -n tee "$panel/brightness" >/dev/null; fi
fi
mark display-ready "brightness=$(<"$panel/brightness")"
unset DISPLAY WAYLAND_DISPLAY
SDL_VIDEODRIVER=kmsdrm SDL_AUDIODRIVER=alsa LD_PRELOAD="$bin/quick-menu.so:$cursor" "$player_bin" & player=$!
mark player-exec "pid=$player"; grep -E '^(MemAvailable|SwapFree):' /proc/meminfo || :
for ((i=0;i<50;i++)); do [[ $(ps -p "$player" -o comm=) = ruffle-keys ]] && break; kill -0 "$player" 2>/dev/null || break; sleep .05; done
python3 "$quit" "$player" --comm ruffle-keys & watcher=$!
mark quit-watch "pid=$watcher player=$player"
rc=0; wait -n -p first "$player" "$watcher" || rc=$?
if [[ $first = "$watcher" ]]; then
  ((rc<=1)) || exit "$rc"
  for ((i=0;i<50;i++)); do kill -0 "$player" 2>/dev/null || break; sleep .1; done
  if kill -0 "$player" 2>/dev/null; then echo 'player did not exit'; exit 1; fi
  rc=0; wait "$player" || rc=$?
else
  kill "$watcher" 2>/dev/null || true; wait "$watcher" 2>/dev/null || true
fi
echo "$(date -Is) exit=$rc"; exit "$rc"
