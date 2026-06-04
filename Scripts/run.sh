#!/bin/bash
clear
DYLIB_PATH="$HOME/.horizon/HorizonMac.dylib"
ROBLOX_PATH="/Applications/Roblox.app/Contents/MacOS/RobloxPlayer"
RELEASE_URL="https://github.com/ajhsjau39/a/releases/latest/download/HorizonMac.dylib"

echo "[*] Downloading latest Horizon..."
mkdir -p "$HOME/.horizon"
curl -L -o "$DYLIB_PATH" "$RELEASE_URL"

if [ ! -f "$DYLIB_PATH" ]; then
    echo "[!] Download failed."
    exit 1
fi

echo "[*] Launching Roblox with Horizon injected..."
DYLD_INSERT_LIBRARIES="$DYLIB_PATH" "$ROBLOX_PATH" &

echo "[*] Waiting for Roblox to start..."
sleep 5

echo "[*] Launching Horizon terminal..."
chmod +x ./horizon.sh
./horizon.sh