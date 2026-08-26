#!/usr/bin/env bash
# install.sh — Build and install the person_mouse kernel module with udev rules.
#
# Usage:
#   sudo ./drivers/install.sh
#
# The device is restricted to a dedicated local group.  Do not grant it
# world-writable permissions: members can inject real pointer and click events.

set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
DEVICE_GROUP="person-mouse"
TARGET_USER="${SUDO_USER:-${PERSON_MOUSE_USER:-}}"
KERNEL_RELEASE="$(uname -r)"
MODULE_DIR="/lib/modules/$KERNEL_RELEASE/extra"
MODULE_PATH="$MODULE_DIR/person_mouse.ko"
AUTOLOAD_PATH="/etc/modules-load.d/person-mouse.conf"

if (( EUID != 0 )); then
    echo "Run this installer with sudo: sudo ./drivers/install.sh" >&2
    exit 2
fi

if [[ -z "$TARGET_USER" ]] || ! id "$TARGET_USER" >/dev/null 2>&1; then
    echo "Run with sudo from the desktop user, or set PERSON_MOUSE_USER." >&2
    exit 2
fi

echo "==> Building kernel module..."
make -C "$SCRIPT_DIR" clean all

echo "==> Granting $TARGET_USER access through the $DEVICE_GROUP group..."
if ! getent group "$DEVICE_GROUP" >/dev/null; then
    sudo groupadd --system "$DEVICE_GROUP"
fi
sudo usermod --append --groups "$DEVICE_GROUP" "$TARGET_USER"

echo "==> Installing restricted udev rule for /dev/person-mouse..."
RULE='KERNEL=="person-mouse", GROUP="person-mouse", MODE="0660"'
printf '%s\n' "$RULE" > /etc/udev/rules.d/99-person-mouse.rules
udevadm control --reload-rules

echo "==> Installing module for kernel $KERNEL_RELEASE..."
install -d -m 0755 "$MODULE_DIR"
install -m 0644 "$SCRIPT_DIR/person_mouse.ko" "$MODULE_PATH"
printf '%s\n' person_mouse > "$AUTOLOAD_PATH"
depmod -a "$KERNEL_RELEASE"

echo "==> Loading module..."
if grep -q '^person_mouse ' /proc/modules; then
    echo "==> Replacing the currently loaded person_mouse module..."
    # Do not force removal: a busy module must be released by its owner first.
    rmmod person_mouse
fi
modprobe person_mouse
udevadm settle --timeout=5

echo "==> Verifying..."
if [[ -c /dev/person-mouse ]]; then
    echo "OK — /dev/person-mouse exists ($(stat -c '%A %U %G' /dev/person-mouse))"
else
    echo "ERROR: /dev/person-mouse not found after loading person_mouse" >&2
    exit 1
fi

echo ""
echo "Done.  Log out and back in before starting SourceSight so $TARGET_USER receives"
echo "the new $DEVICE_GROUP group membership."
echo "The module will load automatically on future boots."
echo "To uninstall:  sudo make -C $SCRIPT_DIR uninstall"
