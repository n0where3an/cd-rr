#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# End-to-end test: protect a set of files with rrenc, damage them, restore
# with rrdec and compare byte-for-byte with the originals.
#
# Usage: roundtrip.sh <path-to-rrenc> <path-to-rrdec>
# Set KEEP=1 to keep the work directory for inspection.

set -u

RRENC=${1:?usage: $0 <rrenc> <rrdec>}
RRDEC=${2:?usage: $0 <rrenc> <rrdec>}

SECTOR=2048		# ISO (mode1/form1) sector size
RR_SECTORS=100	# requested redundancy; rrenc rounds it down to a multiple of 6 (96)

WORK=$(mktemp -d "${TMPDIR:-/tmp}/cd-rr-test.XXXXXX") || exit 1
cleanup() {
	if [ "${KEEP:-0}" = 1 ]; then
		echo "work dir kept: $WORK"
	else
		rm -rf "$WORK"
	fi
}
trap cleanup EXIT

FAILED=0
pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; FAILED=1; }

# zero_sectors <file> <first> <count>: "unreadable" sectors for rrdec -z
zero_sectors() {
	dd if=/dev/zero of="$1" bs=$SECTOR seek="$2" count="$3" conv=notrunc 2>/dev/null
}

# same_files <dir1> <dir2> <names...>
same_files() {
	d1=$1 d2=$2; shift 2
	for f in "$@"; do
		cmp -s "$d1/$f" "$d2/$f" || { echo "  $f differs"; return 1; }
	done
}

# --- protected data set ---------------------------------------------------
ORIG=$WORK/orig
mkdir -p "$ORIG"
head -c 3000000 /dev/urandom > "$ORIG/a.bin"		# 1465 sectors
head -c 1234567 /dev/urandom > "$ORIG/b.bin"		# 603 sectors
head -c 79651 /dev/urandom > "$ORIG/noext"		# 39 sectors, no extension
touch -d '2040-06-01 12:00:00 UTC' "$ORIG/noext"	# after 2038-01-19
FILES="a.bin b.bin noext"

cat > "$WORK/list.lst" <<EOF
-i a.bin
-i b.bin
-i noext
-@
-r
EOF

if "$RRENC" -w -q -@ "$WORK/list.lst" -s "$ORIG" -t "$ORIG" \
		$RR_SECTORS prot.rrt prot.rrd > "$WORK/rrenc.log" 2>&1; then
	pass "rrenc"
else
	cat "$WORK/rrenc.log"; fail "rrenc"; exit 1
fi

if grep -q '^-i  *79651  *2222164800 noext$' "$ORIG/prot.rrt"; then
	pass "rr-list stores 64-bit timestamp"
else
	grep '^-i' "$ORIG/prot.rrt"; fail "rr-list stores 64-bit timestamp"
fi

# run_rrdec <name> <src-dir> <tgt-dir> [extra options]
run_rrdec() {
	name=$1 src=$2 tgt=$3; shift 3
	rm -rf "$tgt"; mkdir -p "$tgt"
	"$RRDEC" -w -q "$@" -s "$src" -@ "$src/prot.rrt" -t "$tgt" \
		> "$WORK/$name.log" 2>&1
}

# --- 1. undamaged copy --------------------------------------------------------
if run_rrdec clean "$ORIG" "$WORK/out1" && same_files "$ORIG" "$WORK/out1" $FILES; then
	pass "undamaged: restored files are identical"
else
	cat "$WORK/clean.log"; fail "undamaged: restored files are identical"
fi

if [ "$(stat -c %Y "$WORK/out1/noext")" = 2222164800 ]; then
	pass "post-2038 mtime restored"
else
	fail "post-2038 mtime restored (got $(stat -c %Y "$WORK/out1/noext"))"
fi

# --- 2. damage within redundancy: 32 bad sectors + a lost file (39) = 71 <= 96
DMG=$WORK/dmg
cp -rp "$ORIG" "$DMG"
zero_sectors "$DMG/a.bin" 10 30
zero_sectors "$DMG/a.bin" 1000 1
zero_sectors "$DMG/b.bin" 5 1
rm "$DMG/noext"
if run_rrdec damaged "$DMG" "$WORK/out2" -z && same_files "$ORIG" "$WORK/out2" $FILES; then
	pass "damaged + missing file: fully recovered"
else
	cat "$WORK/damaged.log"; fail "damaged + missing file: fully recovered"
fi

# --- 3. damage beyond redundancy: must fail and must not leave "good" files ---
zero_sectors "$DMG/a.bin" 100 100
if run_rrdec toomany "$DMG" "$WORK/out3" -z; then
	fail "too many errors: rrdec must fail"
else
	pass "too many errors: rrdec fails"
fi
if [ -e "$WORK/out3/a.bin" ]; then
	fail "too many errors: no unusable file left under its own name"
else
	pass "too many errors: no unusable file left under its own name"
fi

[ $FAILED = 0 ] && echo "all tests passed"
exit $FAILED
