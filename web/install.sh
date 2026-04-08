#!/bin/bash
set -e

echo "=== UltraComb SAL Installer ==="
echo ""

# Get latest release artifacts URL from GitHub API
REPO="joshphoenix1/Ultracomb"
RUN_ID=$(curl -s "https://api.github.com/repos/$REPO/actions/workflows/build-macos.yml/runs?status=success&per_page=1" | python3 -c "import sys,json; print(json.load(sys.stdin)['workflow_runs'][0]['id'])" 2>/dev/null)

if [ -z "$RUN_ID" ]; then
  echo "Error: Could not find a successful build. Check https://github.com/$REPO/actions"
  exit 1
fi

TMPDIR=$(mktemp -d)
cd "$TMPDIR"

echo "Downloading AU plugin..."
# Download via gh CLI if available, otherwise direct API
if command -v gh &>/dev/null; then
  gh run download "$RUN_ID" --repo "$REPO" --name "UltraComb-SAL-macOS-AU" --dir au-download
else
  echo "Install GitHub CLI (gh) for automatic download:"
  echo "  brew install gh && gh auth login"
  echo ""
  echo "Or download manually from:"
  echo "  https://github.com/$REPO/actions/runs/$RUN_ID"
  echo ""
  echo "Then run:"
  echo "  sudo cp -R 'UltraComb SAL.component' /Library/Audio/Plug-Ins/Components/"
  echo "  sudo xattr -cr '/Library/Audio/Plug-Ins/Components/UltraComb SAL.component'"
  rm -rf "$TMPDIR"
  exit 1
fi

echo "Installing to /Library/Audio/Plug-Ins/Components/..."
sudo rm -rf "/Library/Audio/Plug-Ins/Components/UltraComb SAL.component"
sudo cp -R "au-download/UltraComb SAL.component" "/Library/Audio/Plug-Ins/Components/"
sudo xattr -cr "/Library/Audio/Plug-Ins/Components/UltraComb SAL.component"
sudo codesign --force --deep -s - "/Library/Audio/Plug-Ins/Components/UltraComb SAL.component"

# Also install VST3 if available
echo "Downloading VST3 plugin..."
if gh run download "$RUN_ID" --repo "$REPO" --name "UltraComb-SAL-macOS-VST3" --dir vst3-download 2>/dev/null; then
  sudo mkdir -p "/Library/Audio/Plug-Ins/VST3"
  sudo rm -rf "/Library/Audio/Plug-Ins/VST3/UltraComb SAL.vst3"
  sudo cp -R "vst3-download/UltraComb SAL.vst3" "/Library/Audio/Plug-Ins/VST3/"
  sudo xattr -cr "/Library/Audio/Plug-Ins/VST3/UltraComb SAL.vst3"
  sudo codesign --force --deep -s - "/Library/Audio/Plug-Ins/VST3/UltraComb SAL.vst3"
  echo "VST3 installed."
fi

# Restart AU cache
killall -9 AudioComponentRegistrar 2>/dev/null || true

rm -rf "$TMPDIR"

echo ""
echo "=== Done! ==="
echo "AU:   /Library/Audio/Plug-Ins/Components/UltraComb SAL.component"
echo "VST3: /Library/Audio/Plug-Ins/VST3/UltraComb SAL.vst3"
echo ""
echo "Restart your DAW to use UltraComb SAL."
