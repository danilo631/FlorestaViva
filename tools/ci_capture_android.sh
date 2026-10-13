#!/usr/bin/env bash
# Four deterministic SwiftShader captures of the actual GLES3 game, not atlas previews.
set -euo pipefail
mkdir -p build/emulator
CAPTURE_PREFIX="${FOREST_CAPTURE_PREFIX:-before}"
if [ "$CAPTURE_PREFIX" != before ] && [ "$CAPTURE_PREFIX" != after ]; then echo "Invalid capture prefix" >&2; exit 1; fi
trap 'adb logcat -d -v time > build/emulator/full_logcat.txt 2>/dev/null || true; adb logcat -d -v time -s FlorestaViva:E > build/emulator/renderer_metrics_logcat.txt 2>/dev/null || true; adb shell dumpsys gfxinfo com.florestaviva.game > build/emulator/android_gfxinfo.txt 2>/dev/null || true' EXIT
python -m pip install --disable-pip-version-check pillow
adb wait-for-device
adb shell settings put system accelerometer_rotation 0 || true
adb shell settings put system user_rotation 1 || true
adb shell settings put system screen_off_timeout 600000 || true
# Lower software framebuffer footprint for GPU emulation, not real handset benchmarks.
adb shell wm size 720x1280 || true
adb shell wm density 240 || true
adb shell input keyevent KEYCODE_WAKEUP || true
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb logcat -c || true
for scene in 0 1 2 3; do
  adb shell am force-stop com.florestaviva.game || true
  timeout 55s adb shell am start -W -n com.florestaviva.game/.MainActivity --ei captureScene "$scene"
  sleep 11
  adb logcat -d -v time > "build/emulator/scene_${scene}_full_logcat.txt" || true
  if ! adb shell pidof com.florestaviva.game | grep -Eq '[0-9]'; then
      echo "FAIL: Floresta Viva process crashed or exited in scene $scene" >&2
      grep -E "FATAL EXCEPTION|Fatal signal|AndroidRuntime|am_crash|libc:|OutOfMemory"           "build/emulator/scene_${scene}_full_logcat.txt" | tail -80 || true
      exit 11
  fi
  # Ensure the actual game owns the foreground surface, not a crash dialog.
  adb shell dumpsys window windows > "build/emulator/scene_${scene}_window.txt" || true
  if ! grep -E 'mCurrentFocus|mFocusedApp' "build/emulator/scene_${scene}_window.txt"       | grep -q 'com.florestaviva.game'; then
      echo "FAIL: screenshot would capture a launcher/crash dialog, not the game" >&2
      exit 12
  fi
  filename="build/emulator/${CAPTURE_PREFIX}_scene_$scene.png"
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
      { echo "FAIL: game failed to apply deterministic camera"; exit 13; }
done
adb shell am force-stop com.florestaviva.game || true
adb shell am start -W -n com.florestaviva.game/.MainActivity --ei captureScene 0
sleep 4
timeout 45s adb shell screenrecord --size 720x1280 --bit-rate 2000000 --time-limit 8 /sdcard/forest_${CAPTURE_PREFIX}.mp4
adb pull /sdcard/forest_baseline.mp4 build/emulator/${CAPTURE_PREFIX}_walkthrough.mp4
test "$(wc -c < build/emulator/before_walkthrough.mp4)" -gt 15000
adb logcat -d -v time -s FlorestaViva:E > build/emulator/renderer_metrics_logcat.txt || true
adb shell dumpsys gfxinfo com.florestaviva.game > build/emulator/android_gfxinfo.txt || true
echo "Four game screenshots, screenrecord and logcat generated; SwiftShader is NOT an Android GPU benchmark."
