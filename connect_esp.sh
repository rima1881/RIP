#!/usr/bin/env bash
#
# connect_esp.sh — pair-free connect to the ESP32 SPP server and bind /dev/rfcomm0.
#
# The ESP32 firmware runs a Bluetooth Classic SPP server with no pairing/security
# and advertises the name "RIPBridge". This script discovers it, clears any stale
# RFCOMM state, and binds it to /dev/rfcomm0 (channel 1).
#
# Usage:
#   ./connect_esp.sh [MAC]
#
# With no argument, it auto-discovers the device by name. If discovery fails,
# pass the Bluetooth MAC explicitly, e.g.:
#   ./connect_esp.sh 78:1C:3C:A9:D8:4E
#
# Requires: hcitool, rfcomm, sudo.

set -euo pipefail

DEVICE_NAME="RIPBridge"
RFCOMM_DEV="0"
RFCOMM_CHANNEL="1"
RFCOMM_PATH="/dev/rfcomm${RFCOMM_DEV}"

# --- helpers -------------------------------------------------------------

log() { printf '\033[1;32m[connect_esp]\033[0m %s\n' "$*"; }
err() { printf '\033[1;31m[connect_esp]\033[0m %s\n' "$*" >&2; }

sudo_cmd() {
    # Prefer non-interactive sudo if a password is cached; fall back to prompt.
    if sudo -n true 2>/dev/null; then
        sudo "$@"
    else
        sudo "$@"
    fi
}

# --- argument / discovery -------------------------------------------------

MAC="${1:-}"

if [[ -z "$MAC" ]]; then
    log "Discovering '$DEVICE_NAME' ..."
    # hcitool scan prints "<mac>\t<name>". Grep by name and take the address.
    MAC="$(timeout 30 hcitool scan 2>/dev/null | awk -v n="$DEVICE_NAME" 'NF>=2 && $0 ~ n { print $1; exit }')"

    if [[ -z "$MAC" ]]; then
        err "Could not find '$DEVICE_NAME' in the scan."
        err "Is the ESP32 powered and running the firmware? Try passing the MAC explicitly."
        exit 1
    fi
fi

# Upper-case the MAC for uniformity.
MAC="$(printf '%s' "$MAC" | tr 'a-f' 'A-F')"
log "Using device $MAC"

# --- clear stale state ----------------------------------------------------

log "Clearing stale RFCOMM state ..."
sudo_cmd rfcomm release "$RFCOMM_DEV" 2>/dev/null || true
sudo_cmd rfcomm release all 2>/dev/null || true

# Restart bluetooth only if the device is currently stuck in a busy/disconnecting
# state; otherwise leave it alone to avoid needlessly dropping other devices.
if rfcomm show all 2>/dev/null | grep -qiE 'disconnecting|connected'; then
    log "Stale RFCOMM DLC detected; restarting bluetooth ..."
    sudo_cmd systemctl restart bluetooth
    sleep 3
fi

# --- bind ----------------------------------------------------------------

log "Binding $MAC to $RFCOMM_PATH (channel $RFCOMM_CHANNEL) ..."
sudo_cmd rfcomm bind "$RFCOMM_DEV" "$MAC" "$RFCOMM_CHANNEL"

# Make the device accessible to the current user.
if [[ -e "$RFCOMM_PATH" ]]; then
    sudo_cmd chmod 666 "$RFCOMM_PATH"
fi

log "Done. $RFCOMM_PATH is ready."
log "Run:  ./build/bridge_test --port $RFCOMM_PATH --steps 10 --torque 0.5"
