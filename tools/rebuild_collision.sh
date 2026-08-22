#!/bin/bash
#
# Rebuild the compiled collision of every listed mesh with NifSkope's own writer
# and stage it as a Mod Organizer mod.
#
#   decompile every compiled system -> compile every editable body back
#
# so what ships is collision this program WROTE, which is the thing being tested.
# A file that will not round-trip cleanly is left out rather than shipped broken:
# a crash then means our bytes, not a mesh we mangled on the way in.
#
# USAGE
#   bash rebuild.sh <list.tsv> <mod-dir> [limit]
#
# list.tsv is "placements<TAB>relative\path.nif" from the ESM pass. It is written
# by Python on Windows, so it arrives CRLF and with the ESM's own backslashes;
# both are dealt with up front rather than per line.

set -u
R="${NIFROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
NS="${EXE:-$R/release/NifSkope.exe}"
SRC="${VAN:-E:/Tools/Fallout 4/DataUnpacked/Data/Meshes}"
LIST="$1"
MOD="$2"
LIMIT="${3:-0}"
# One worker per partition writes its own manifest and they are concatenated
# afterwards: the resume check greps THIS file, so sharing one across workers
# would make each skip the others' rows. The mesh outputs never collide -- a
# partition owns whole paths.
MAN="${MANIFEST:-$MOD/collision_manifest.tsv}"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT

tr -d '\r' < "$LIST" | tr '\\' '/' > "$W/list.txt"

mkdir -p "$MOD/Meshes"
[ -f "$MAN" ] || printf 'status\tplacements\tsystems\tshapes\trelative_path\tnote\n' > "$MAN"

done_n=0; ok=0; skipped=0
while IFS=$'\t' read -r places rel; do
	[ -n "${rel:-}" ] || continue
	done_n=$((done_n+1))
	[ "$LIMIT" = "0" ] || [ "$done_n" -le "$LIMIT" ] || break
	# already recorded? then this is a resumed run and it is done
	if grep -qF "	$rel	" "$MAN" 2>/dev/null; then continue; fi

	in="$SRC/$rel"
	[ -f "$in" ] || { printf 'missing\t%s\t0\t0\t%s\t-\n' "$places" "$rel" >> "$MAN"; skipped=$((skipped+1)); continue; }

	was=$("$NS" -no-gui list "$in" 2>/dev/null | grep -c 'bhkNPCollisionObject')
	[ "${was:-0}" -gt 0 ] || { printf 'nocollision\t%s\t0\t0\t%s\t-\n' "$places" "$rel" >> "$MAN"; skipped=$((skipped+1)); continue; }

	rm -f "$W/a.nif" "$W/b.nif"
	"$NS" -no-gui cast "$in" -s "Havok/Decompile All Compiled Collision" -o "$W/a.nif" >/dev/null 2>&1
	[ -s "$W/a.nif" ] || { printf 'decompile-failed\t%s\t%s\t0\t%s\t-\n' "$places" "$was" "$rel" >> "$MAN"; skipped=$((skipped+1)); continue; }

	# compile every editable body; block numbers move under us, so re-list each time
	note='-'
	for _ in $(seq 1 40); do
		obj=$("$NS" -no-gui list "$W/a.nif" 2>/dev/null | sed -n 's/^\[\([0-9]*\)\] bhkCollisionObject.*/\1/p' | head -1)
		[ -n "$obj" ] || break
		msg=$("$NS" -no-gui cast "$W/a.nif" -s "Havok/Compile Collision" -b "$obj" -o "$W/b.nif" 2>&1)
		case "$msg" in *declined*) note="declined";; esac
		[ -s "$W/b.nif" ] || { note="compile-refused"; break; }
		mv -f "$W/b.nif" "$W/a.nif"
	done

	/bin/true
	# Compile appends its blocks, so the collision lands at the END of the file
	# where vanilla keeps it beside the node that owns it. Every reference is
	# re-pointed correctly and the file is self-consistent either way -- but
	# vanilla puts it mid-list in 13 of 13 doors, and the doors are what stopped
	# working. Sanitize/Reorder Blocks restores the canonical order exactly:
	# block-for-block identical to vanilla, refs unchanged, collision still
	# decoding, body position intact.
	"$NS" -no-gui cast "$W/a.nif" -s "Sanitize/Reorder Blocks" -o "$W/r.nif" >/dev/null 2>&1
	[ -s "$W/r.nif" ] && mv -f "$W/r.nif" "$W/a.nif"

	left=$("$NS" -no-gui list "$W/a.nif" 2>/dev/null | grep -c 'bhkCollisionObject')
	now=$("$NS" -no-gui list "$W/a.nif" 2>/dev/null | grep -c 'bhkNPCollisionObject')
	shapes=$("$NS" -no-gui collision "$W/a.nif" 2>/dev/null | awk '$2 ~ /^hknp/' | grep -c .)
	if [ "$note" = "compile-refused" ] || [ "${left:-1}" != "0" ] || [ "${now:-0}" != "$was" ]; then
		printf 'refused\t%s\t%s\t%s\t%s\t%s\n' "$places" "$was" "${shapes:-0}" "$rel" "$note left=$left now=$now" >> "$MAN"
		skipped=$((skipped+1)); continue
	fi
	# the independent byte check; exit 2 just means there is no compressed mesh in it
	python "$R/tools/hkmatrun.py" "$W/a.nif" --quiet >/dev/null 2>&1
	if [ "$?" = "1" ]; then
		printf 'runtable-fail\t%s\t%s\t%s\t%s\t-\n' "$places" "$was" "${shapes:-0}" "$rel" >> "$MAN"
		skipped=$((skipped+1)); continue
	fi
	# and the compound check, read the way the ENGINE reads it: a compound that
	# does not follow its own node-array pointer is the crash of 2026-08-21, and
	# one whose AABB is not the root box is a body the broadphase mis-sizes. 2
	# means the file holds no compound, which is most of them.
	python "$R/tools/hkcompound.py" "$W/a.nif" --quiet >/dev/null 2>&1
	if [ "$?" = "1" ]; then
		printf 'compound-fail\t%s\t%s\t%s\t%s\t-\n' "$places" "$was" "${shapes:-0}" "$rel" >> "$MAN"
		skipped=$((skipped+1)); continue
	fi
	mkdir -p "$MOD/Meshes/$(dirname "$rel")"
	cp -f "$W/a.nif" "$MOD/Meshes/$rel"
	printf 'ok\t%s\t%s\t%s\t%s\t%s\n' "$places" "$was" "${shapes:-0}" "$rel" "$note" >> "$MAN"
	ok=$((ok+1))
done < "$W/list.txt"
echo "rebuilt $ok, skipped $skipped, of $done_n considered"
