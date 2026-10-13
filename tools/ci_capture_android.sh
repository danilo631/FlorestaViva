#!/usr/bin/env bash
# Four deterministic SwiftShader captures of the actual GLES3 game, not atlas previews.
set -euo pipefail
mkdir -p build/emulator
trap 'adb logcat -d -v time > build/emulator/full_logcat.txt 2>/dev/null || true; adb logcat -d -v time -s FlorestaViva:E > build/emulator/renderer_metrics_logcat.txt 2>/dev/null || true; adb shell dumpsys gfxinfo com.florestaviva.game > build/emulator/android_gfxinfo.txt 2>/dev/null || true' EXIT
python -m pip install --disable-pip-version-check pillow
adb wait-for-device
adb shell settings put system accelerometer_rotation 0 || true
adb shell settings put system user_rotation 1 || true
adb shell settings put system screen_off_timeout 600000 || true
adb shell wm size 720x1280 || true
adb shell wm density 240 || true
adb shell input keyevent KEYCODE_WAKEUP || true
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb logcat -c || true
for scene in 0 1 2 3; do
  adb shell am force-stop com.florestaviva.game || true
  timeout 55s adb shell am start -W -n com.florestaviva.game/.MainActivity --ei captureScene "$scene"
  # SwiftShader may require >30s to create the first valid GLES surface.
  # Avoid capturing the launcher while Android is still awaiting its first draw.
  displayed=false
  for attempt in $(seq 1 21); do
    sleep 4
    if ! adb shell pidof com.florestaviva.game | grep -Eq '[0-9]'; then
      echo "FAIL: app process exited during scene $scene startup (attempt $attempt)" >&2
      break
    fi
    adb shell dumpsys window windows > "build/emulator/scene_${scene}_window.txt" || true
    if grep -E 'mCurrentFocus|mFocusedApp' "build/emulator/scene_${scene}_window.txt" |
          grep -q 'com.florestaviva.game'; then
      displayed=true
      break
    fi
  done
  adb logcat -d -v time > "build/emulator/scene_${scene}_full_logcat.txt" || true
  if [ "$displayed" != true ]; then
      echo "FAIL: game never reached foreground surface scene $scene within 84 seconds" >&2
      grep -E "FATAL EXCEPTION|Fatal signal|AndroidRuntime|am_crash|ANR|OutOfMemory" \
          "build/emulator/scene_${scene}_full_logcat.txt" | tail -90 || true
      exit 11
  fi
  # Do not mistake Android's first UI frame for the actual native 3D render.
  rendered=false
  for attempt in $(seq 1 15); do
    if adb logcat -d -s FlorestaViva:E | grep -q "CI_RENDER_FRAME scene=$scene"; then
        rendered=true
        break
    fi
    sleep 3
  done
  if [ "$rendered" != true ]; then
      echo "FAIL: native GLES renderer never finished frame for scene $scene" >&2
      adb logcat -d -v time > "build/emulator/scene_${scene}_full_logcat.txt" || true
      exit 15
  fi
  sleep 3
  if ! adb shell pidof com.florestaviva.game | grep -Eq '[0-9]'; then
      echo "FAIL: Floresta Viva exited before scene $scene captured" >&2
      grep -E "FATAL EXCEPTION|Fatal signal|AndroidRuntime|am_crash|libc:|OutOfMemory"           "build/emulator/scene_${scene}_full_logcat.txt" | tail -100 || true
      exit 11
  fi
  adb shell dumpsys window windows > "build/emulator/scene_${scene}_window.txt" || true
  if ! grep -E 'mCurrentFocus|mFocusedApp' "build/emulator/scene_${scene}_window.txt"        | grep -q 'com.florestaviva.game'; then
      echo "FAIL: emulator showing launcher/crash dialog, not Floresta Viva" >&2
      exit 12
  fi
  filename="build/emulator/before_scene_$scene.png"
  timeout 40s adb exec-out screencap -p > "$filename"
  python - "$filename" <<'PY'
from PIL import Image,ImageStat
import sys
im=Image.open(sys.argv[1]).convert("RGB")
assert im.width>=640 and im.height>=480,im.size
stat=ImageStat.Stat(im.resize((64,64)))
assert max(stat.stddev)>5.0,("black/blank screenshot",sys.argv[1],stat.stddev)
print("EMULATOR_SCREENSHOT",sys.argv[1],im.size,"stddev",stat.stddev,flush=True)
PY
  adb logcat -d -s FlorestaViva:E > "build/emulator/log_scene_$scene.txt" || true
  grep "CI_CAPTURE scene=$scene" "build/emulator/log_scene_$scene.txt" ||
    { echo "FAIL: debug camera not positioned"; exit 13; }
done
adb shell am force-stop com.florestaviva.game || true
adb shell am start -W -n com.florestaviva.game/.MainActivity --ei captureScene 0
sleep 4
timeout 45s adb shell screenrecord --size 720x1280 --bit-rate 2000000 --time-limit 8 /sdcard/forest_baseline.mp4
adb pull /sdcard/forest_baseline.mp4 build/emulator/before_walkthrough.mp4
test "$(wc -c < build/emulator/before_walkthrough.mp4)" -gt 15000
adb logcat -d -v time -s FlorestaViva:E > build/emulator/renderer_metrics_logcat.txt || true
adb shell dumpsys gfxinfo com.florestaviva.game > build/emulator/android_gfxinfo.txt || true
echo "Four game screenshots, screenrecord and logcat generated; SwiftShader is NOT an Android GPU benchmark."
