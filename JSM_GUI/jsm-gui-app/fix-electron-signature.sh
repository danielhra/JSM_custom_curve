#!/bin/bash
# Dev-environment helper, macOS only.
#
# Electron's darwin zips ship the runtime only linker-signed, with no sealed
# resources. On macOS 15+ that counts as a broken signature, so Gatekeeper
# refuses to launch it and offers to move it to the Bin instead. Giving the
# bundle a valid ad-hoc signature fixes it.
#
# This has to run from a terminal you have granted "App Management" to
# (System Settings > Privacy & Security > App Management) because modifying
# another app's bundle is protected; macOS will prompt the first time.
#
# Re-run this after any `npm ci` / `npm install` that reinstalls electron.
set -uo pipefail

APP="$(cd "$(dirname "$0")" && pwd)/node_modules/electron/dist/Electron.app"

if [[ ! -d "$APP" ]]; then
  echo "Electron not found at $APP"
  echo "Run 'npm ci' first."
  exit 1
fi

echo "Signing $APP"

# Deepest first: --deep is deprecated and mishandles nested bundles.
find "$APP" \( -name "*.dylib" -o -name "*.so" \) -print0 |
  while IFS= read -r -d '' lib; do
    codesign --force --sign - "$lib" 2>&1 | grep -v "replacing existing signature"
  done

for helper in "$APP/Contents/Frameworks/"*.app; do
  [[ -e "$helper" ]] || continue
  codesign --force --sign - "$helper" 2>&1 | grep -v "replacing existing signature"
done

for framework in "$APP/Contents/Frameworks/"*.framework; do
  [[ -e "$framework" ]] || continue
  codesign --force --sign - "$framework/Versions/A" 2>&1 | grep -v "replacing existing signature"
done

codesign --force --sign - "$APP" 2>&1 | grep -v "replacing existing signature"

echo
if codesign --verify --strict "$APP"; then
  echo "Signature is valid. 'npm run dev' should launch now."
else
  echo "Still invalid. If you saw 'Operation not permitted' above, grant this"
  echo "terminal App Management in System Settings > Privacy & Security."
  exit 1
fi
