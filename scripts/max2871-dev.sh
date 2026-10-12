#!/usr/bin/env bash
#
# max2871-dev.sh - development loop for the MAX2871 driver (OpenWrt 24.10, Pi 3B+)
#
#   1. Sync sources: my_own_driver/  ->  openwrt-24.10/package/kernel/max2871/
#   2. Build:        make package/kernel/max2871/compile
#   3. checkpatch:   print the error/warning summary
#   4. (optional)    Copy max2871.ko, etc/modules.d/max2871 and max2871.dtbo to the SD card
#
# Usage:
#   ./max2871-dev.sh                 # steps 1-3
#   ./max2871-dev.sh /dev/sdX        # steps 1-4, SD card is /dev/sdX
#
# The script only mounts, copies, syncs and unmounts. It never formats or dd's.

set -euo pipefail

SRC_DIR="$HOME/workspace/fortes/max2871/my_own_driver"
OWRT="$HOME/workspace/openwrt-24.10"
PKG_DIR="$OWRT/package/kernel/max2871"
BUILD_ROOT="$OWRT/build_dir/target-aarch64_cortex-a53_musl/linux-bcm27xx_bcm2710"
PKG_BUILD="$BUILD_ROOT/max2871"

LINUX_DIR=$(ls -d "$BUILD_ROOT"/linux-[0-9]* | head -n1)
KVER=${LINUX_DIR##*/linux-}

step() { printf '\n==> %s\n' "$*"; }
die()  { printf 'ERROR: %s\n' "$*" >&2; exit 1; }

# ---------------------------------------------------------------- 1. sync
step "Syncing sources to $PKG_DIR"
# All .c/.h in src/ plus the Kbuild Makefile and the overlay
SYNC_FILES=(src/Makefile files/max2871-overlay.dts)
for p in "$SRC_DIR"/src/*.c "$SRC_DIR"/src/*.h; do
	[ -e "$p" ] || continue
	SYNC_FILES+=("src/$(basename "$p")")
done
for f in "${SYNC_FILES[@]}"; do
	[ -f "$SRC_DIR/$f" ] || die "missing $SRC_DIR/$f"
	if ! cmp -s "$SRC_DIR/$f" "$PKG_DIR/$f"; then
		cp "$SRC_DIR/$f" "$PKG_DIR/$f"
		echo "  updated $f"
	fi
done
# Remove package sources that no longer exist in my_own_driver (e.g. renamed files)
REMOVED=0
for p in "$PKG_DIR"/src/*.c "$PKG_DIR"/src/*.h; do
	[ -e "$p" ] || continue
	f="src/$(basename "$p")"
	if [[ ! " ${SYNC_FILES[*]} " == *" $f "* ]]; then
		rm -f "$p"
		echo "  removed $f"
		REMOVED=1
	fi
done
# The package build dir keeps stale copies of removed files; start it fresh
if [ "$REMOVED" = 1 ]; then
	echo "  cleaning package build dir"
	make -C "$OWRT" package/kernel/max2871/clean >/dev/null 2>&1
fi

# ---------------------------------------------------------------- 2. build
step "Building package (kernel $KVER)"
LOG=$(mktemp)
if ! make -C "$OWRT" package/kernel/max2871/compile V=s >"$LOG" 2>&1; then
	grep -E "error:|warning:|Error " "$LOG" || tail -n 30 "$LOG"
	rm -f "$LOG"
	die "build failed"
fi
if grep -E "error:|warning:" "$LOG"; then
	rm -f "$LOG"
	die "build produced errors/warnings"
fi
rm -f "$LOG"
ls -l --time-style=+%H:%M:%S "$PKG_BUILD/max2871.ko" "$PKG_BUILD/max2871.dtbo"

# ---------------------------------------------------------------- 3. checkpatch
step "checkpatch"
CP_OUT=$("$LINUX_DIR/scripts/checkpatch.pl" --no-tree -q -f "$PKG_DIR"/src/*.c "$PKG_DIR"/src/*.h || true)
grep '^total' <<<"$CP_OUT" || echo "  0 errors, 0 warnings"

# ---------------------------------------------------------------- 4. SD card
[ $# -ge 1 ] || { step "Done (no SD card given, nothing copied)"; exit 0; }

DEV=$1
step "Copying to SD card $DEV"
[ -b "$DEV" ] || die "$DEV is not a block device"

# lsblk pads column values with spaces (e.g. " 1"), strip them before comparing
lsval() { lsblk "$@" | tr -d '[:space:]'; }
[ "$(lsval -dno RM "$DEV")" = "1" ] || die "$DEV is not removable; refusing to touch a fixed disk"
P1=${DEV}1
P2=${DEV}2
[ "$(lsval -no FSTYPE "$P1")" = "vfat" ] || die "$P1 is not vfat (expected OpenWrt boot partition)"
[ "$(lsval -no FSTYPE "$P2")" = "ext4" ] || die "$P2 is not ext4 (expected OpenWrt ext4 rootfs)"

# Unmount desktop auto-mounts, if any
for p in "$P1" "$P2"; do
	if findmnt -rn -S "$p" >/dev/null; then
		sudo umount "$p"
	fi
done

MNT=$(mktemp -d)
cleanup() { sudo umount "$MNT" 2>/dev/null || true; rmdir "$MNT"; }
trap cleanup EXIT

sudo mount "$P2" "$MNT"
[ -d "$MNT/lib/modules/$KVER" ] || die "rootfs on card has no lib/modules/$KVER (different kernel image?)"
sudo cp "$PKG_BUILD/max2871.ko" "$MNT/lib/modules/$KVER/"
echo "  $P2: lib/modules/$KVER/max2871.ko"
# IIO version: max2871.ko depends on industrialio.ko (kmod-iio-core).
# Copy it too, so a card flashed before IIO was enabled still works.
IIO_PKG="$BUILD_ROOT/packages/ipkg-aarch64_cortex-a53/kmod-iio-core"
if modinfo -F depends "$PKG_BUILD/max2871.ko" | grep -qw industrialio; then
	sudo cp "$LINUX_DIR/drivers/iio/industrialio.ko" "$MNT/lib/modules/$KVER/"
	sudo install -D -m 0644 "$IIO_PKG/etc/modules.d/55-iio-core" "$MNT/etc/modules.d/55-iio-core"
	echo "  $P2: lib/modules/$KVER/industrialio.ko + etc/modules.d/55-iio-core"
fi
# AUTOLOAD: kmodloader loads every module listed in /etc/modules.d at boot
MODD="$PKG_BUILD/ipkg-aarch64_cortex-a53/kmod-max2871/etc/modules.d/max2871"
if [ -f "$MODD" ]; then
	sudo install -D -m 0644 "$MODD" "$MNT/etc/modules.d/max2871"
	echo "  $P2: etc/modules.d/max2871 (autoload at boot)"
fi
sync
sudo umount "$MNT"

sudo mount "$P1" "$MNT"
[ -d "$MNT/overlays" ] || die "boot partition has no overlays/ directory"
sudo cp "$PKG_BUILD/max2871.dtbo" "$MNT/overlays/"
echo "  $P1: overlays/max2871.dtbo"
grep -q '^dtoverlay=max2871' "$MNT/config.txt" \
	|| echo "  WARNING: config.txt on the card has no 'dtoverlay=max2871' line"
sync
sudo umount "$MNT"

step "Done. Move the card to the Pi and boot; the driver loads automatically (dmesg | grep max2871)"
