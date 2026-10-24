#!/bin/bash
# Quit, reinstall into /Applications, verify, relaunch.
#
# Doing this by hand is error-prone: rm -rf fails while the app is running,
# so "rm -rf ... && ditto ..." silently does nothing and leaves the old build
# in place with a valid signature. And cp -R does not preserve the nested
# bundle seals a code signature depends on, so ditto is required.
set -uo pipefail

SRC="$(cd "$(dirname "$0")" && pwd)/release/2.1.0/mac-arm64/JoyShockMapper Custom Curve.app"
DST="/Applications/JoyShockMapper Custom Curve.app"

[[ -d "$SRC" ]] || { echo "No build at $SRC - run: npm run build"; exit 1; }

if pgrep -f "JoyShockMapper Custom Curve" >/dev/null; then
  echo "Stopping the running app..."
  pkill -TERM -f "JoyShockMapper Custom Curve"
  pkill -TERM -f "bin/SDL/JoyShockMapper"
  for _ in $(seq 1 20); do
    pgrep -f "JoyShockMapper Custom Curve" >/dev/null || break
    sleep 0.5
  done
  if pgrep -f "JoyShockMapper Custom Curve" >/dev/null; then
    echo "Could not stop it; quit it manually and re-run."
    exit 1
  fi
fi

rm -rf "$DST"
if [[ -e "$DST" ]]; then
  echo "Could not remove $DST - is something still using it?"
  exit 1
fi

ditto "$SRC" "$DST" || { echo "ditto failed"; exit 1; }

if codesign --verify --strict "$DST" >/dev/null 2>&1; then
  echo "Installed, signature valid."
else
  echo "WARNING: signature is invalid after install."
  exit 1
fi

echo "Active profile:"
grep -hE "^(R = |CAPTURE = |GYRO_OFF|ADAPTIVE_TRIGGER|REAL_WORLD_CALIBRATION|IN_GAME_SENS|MIN_GYRO_SENS|MAX_GYRO_SENS)" \
  "$DST/Contents/Resources/bin/profiles-library/Profile 1.txt" | sed 's/^/  /'

open "$DST" && echo "Relaunched."
